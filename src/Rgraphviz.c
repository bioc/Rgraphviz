#include "common.h"
#include "circle.h"
SEXP R_scalarReal(double v) {
    SEXP ans = allocVector(REALSXP,1);
    REAL(ans)[0] = v;
    return(ans);
}

SEXP R_scalarInteger(int v)
{
  SEXP  ans = allocVector(INTSXP, 1);
  INTEGER(ans)[0] = v;
  return(ans);
}

SEXP
R_scalarLogical(Rboolean v)
{
  SEXP  ans = allocVector(LGLSXP, 1);
  LOGICAL(ans)[0] = v;
  return(ans);
}

SEXP 
R_scalarString(const char *v)
{
  SEXP ans = allocVector(STRSXP, 1);
  PROTECT(ans);
  if(v)
    SET_STRING_ELT(ans, 0, mkChar(v));
  UNPROTECT(1);
  return(ans);
}

SEXP getListElement(SEXP list, char *str) {
    /* Given a R list and acharacter string, will return the */
    /* element of the list which has the name that corresponds to the */
    /*   string */
    SEXP elmt = R_NilValue, names = getAttrib(list, R_NamesSymbol);
    int i;

    for (i = 0; i < length(list); i++)
	if (strcmp(CHAR(STRING_ELT(names,i)), str) == 0) {
	    elmt = VECTOR_ELT(list, i);
	    break;
	}
    return(elmt);
}

int getVectorPos(SEXP vector, char *str) {
    /* Returns position in a vector that matches string name */
    /* Returns -1 if not found */
    
    SEXP names = getAttrib(vector, R_NamesSymbol);
    int i;

    for (i = 0; i < length(vector); i++)
	if (strcmp(CHAR(STRING_ELT(names,i)),str) == 0)
	    break;

    if (i == length(vector))
	i = -1;

    return(i);
}

static SEXP Rgraphviz_graph_type_tag;

#define CHECK_Rgraphviz_graph(s) do { \
     if (TYPEOF(s) != EXTPTRSXP || \
         R_ExternalPtrTag(s) != Rgraphviz_graph_type_tag) \
         error("bad graph reference"); \
} while (0)

SEXP Rgraphviz_init(void) {
    Rgraphviz_graph_type_tag = install("RGRAPH_TYPE_TAG");

    /* Stifle graphviz warning messages, only return errors */
    agseterr(AGERR);

    gvc = gvNEWcontext(Info, "");

    return(R_NilValue);
}

SEXP Rgraphviz_fin(SEXP s) { 
    /* Finalizer for the external reference */
    Agraph_t *g;

    CHECK_Rgraphviz_graph(s);
    g = R_ExternalPtrAddr(s);
    agclose(g); 
    R_ClearExternalPtr(s);
    return(R_NilValue);
}


SEXP Rgraphviz_agread(SEXP filename) {
    Agraph_t *g;
    FILE *dotFile;

    dotFile = fopen(STR(filename),"r");
    if (dotFile == NULL) {
	error("Requested file does not exit");
    }
    aginit();
    g = agread(dotFile);
    return(buildRagraph(g));
}

SEXP Rgraphviz_agwrite(SEXP graph, SEXP filename) {
    /* Takes an R graph and writes it out in DOT format to a file */
    Agraph_t *g;
    FILE *dotFile;
    SEXP slotTmp;

    /* extract the Agraph_t* external reference from the R object */
    slotTmp = GET_SLOT(graph, Rf_install("agraph"));
    CHECK_Rgraphviz_graph(slotTmp);
    g = R_ExternalPtrAddr(slotTmp);

    /* output the Agraph_t */
    dotFile = fopen(STR(filename),"w");
    if (dotFile == NULL) {
	error("Error opening file");
    }
    agwrite(g, dotFile);

    fclose(dotFile);
    return(R_NilValue);
}
    

SEXP Rgraphviz_agopen(SEXP name, SEXP kind, SEXP nodes, 
		     SEXP edges, SEXP attrs, SEXP subGs) {
    /* Will create a new Agraph_t* object in graphviz and then */
    /* a Ragraph S4 object around it, returning it to R */
    Agraph_t *g, *tmpGraph;
    Agraph_t **sgs;
    Agnode_t *head, *tail, *tmp;
    Agedge_t *curEdge;
    int ag_k = 0;
    int i,j;
    int curSubG;

    SEXP pNode, curPN, pEdge, curPE;
    SEXP attrNames, curAttrs;

    pNode = MAKE_CLASS("pNode");
    pEdge = MAKE_CLASS("pEdge");
    
    if (!isInteger(kind))
	error("kind must be an integer value");
    else
	ag_k = INTEGER(kind)[0];

    if ((ag_k < 0)||(ag_k > 3))
	error("kind must be an integer value between 0 and 3");

    if (!isString(name))
	error("name must be a string");

    aginit();
    g = agopen(STR(name), ag_k);

    /* Allocate space in the subgraph array */
    sgs = (Agraph_t **)R_alloc(length(subGs), sizeof(Agraph_t *));
    if ((length(subGs) > 0) && (sgs == NULL))
	error("Out of memory while allocating subgraphs");

    if (length(subGs) > 0) { 
	/* Create any subgraphs, if necessary */	
	for (i = 0; i < length(subGs); i++) {
	    sgs[i] = agsubg(g,CHAR(STRING_ELT(subGs,i)));
	}
    }

    /* Set default attributes */
    g = setDefaultAttrs(g,attrs);

    /* Get the nodes created */
    for (i = 0; i < length(nodes); i++) {
	PROTECT(curPN = VECTOR_ELT(nodes, i));

	/* Need to check the node # against the subG vector */
	/* And assign it to the proper graph, not necessarily 'g' */
	curSubG = INTEGER(GET_SLOT(curPN, Rf_install("subG")))[0];
	if (curSubG > 0) {
	    /* Point tmpGraph to the appropriate current graph */
	    /* Remember that in R they're numbered 1->X and in */
	    /* C it is 0-(X-1) */
	    tmpGraph = sgs[curSubG-1];
	}
	else 
	    tmpGraph = g;
	
	tmp = agnode(tmpGraph, STR(GET_SLOT(curPN, 
					     Rf_install("name"))));

	PROTECT(curAttrs = GET_SLOT(curPN, Rf_install("attrs")));
	PROTECT(attrNames = getAttrib(curAttrs, R_NamesSymbol));
	for (j = 0; j < length(curAttrs); j++) {
	    agset(tmp,  CHAR(STRING_ELT(attrNames,j)),
		  STR(VECTOR_ELT(curAttrs,j)));
	}

	UNPROTECT(3);
    }

    /* now fill in the edges */
    for (i = 0; i < length(edges); i++) {
	PROTECT(curPE = VECTOR_ELT(edges, i));

	curSubG = INTEGER(GET_SLOT(curPE, Rf_install("subG")))[0];
  	if (curSubG > 0) {
	    tmpGraph = sgs[curSubG-1];
	}
	else { 
	    tmpGraph = g;
	} 

	tail = agfindnode(g, STR(GET_SLOT(curPE,
					  Rf_install("from"))));
	if (tail == NULL)
	    error("Missing tail node");

	head = agfindnode(g, STR(GET_SLOT(curPE, 
					   Rf_install("to"))));
	if (head == NULL)
	    error("Missing head node");

	curEdge = agedge(tmpGraph, tail, head);

	PROTECT(curAttrs = GET_SLOT(curPE, Rf_install("attrs")));
	PROTECT(attrNames = getAttrib(curAttrs, R_NamesSymbol));
	for (j = 0; j < length(curAttrs); j++) {
	    agset(curEdge, CHAR(STRING_ELT(attrNames,j)),
		  STR(VECTOR_ELT(curAttrs, j)));
	}
	UNPROTECT(3);
    }

    gvc->g = g;
    GD_gvc(g) = gvc;

    return(buildRagraph(g));    
}

SEXP Rgraphviz_getAttr(SEXP graph, SEXP attr) {
    Agraph_t *g;
    SEXP slotTmp;

    PROTECT(slotTmp = GET_SLOT(graph, install("agraph")));
    CHECK_Rgraphviz_graph(slotTmp);
    g = R_ExternalPtrAddr(slotTmp);
    UNPROTECT(1);

    return(R_scalarString(agget(g, STR(attr))));
}
    

SEXP Rgraphviz_doLayout(SEXP graph, SEXP layoutType) {
    /* Will perform a Graphviz layout on a graph */
    Agraph_t *g;
    Rboolean laidout;
    SEXP slotTmp, nLayout, cPoints, bb;

    /* First make sure that hte graph is not already laid out */
    laidout = (int)LOGICAL(GET_SLOT(graph, Rf_install("laidout")))[0];
    if (laidout == FALSE) {
	/* Extract the Agraph_t pointer from the S4 object */
	PROTECT(slotTmp = GET_SLOT(graph, install("agraph")));
	CHECK_Rgraphviz_graph(slotTmp);
	g = R_ExternalPtrAddr(slotTmp);

	/* Call the appropriate Graphviz layout routine */
	if (!isInteger(layoutType))
	    error("layoutType must be an integer value");
	else {
	    /* Note that we're using the standard dotneato */
	    /* layout commands for layouts and not the ones */
	    /* provided below.  This is a test */
	    switch(INTEGER(layoutType)[0]) {
	    case DOTLAYOUT:
		dot_layout(g);
		break;
	    case NEATOLAYOUT:
		neato_layout(g);
		break;
	    case TWOPILAYOUT:
		twopi_layout(g);
		break;
	    default:
		error("Invalid layout type\n");
	    }
	}

	/* Here we want to extract information for the resultant S4
	   object */
	PROTECT(nLayout = getNodeLayouts(g));
	PROTECT(bb = getBoundBox(g));
	PROTECT(cPoints= 
		getEdgeLocs(g, INTEGER(GET_SLOT(graph, 
						Rf_install("numEdges")))[0]));
	SET_SLOT(graph, Rf_install("agraph"), slotTmp);
	SET_SLOT(graph,Rf_install("AgNode"),nLayout);
	SET_SLOT(graph,Rf_install("laidout"), R_scalarLogical(TRUE));
	SET_SLOT(graph,Rf_install("AgEdge"), cPoints);
	SET_SLOT(graph,Rf_install("boundBox"), bb);
	UNPROTECT(4);
    }

    return(graph);
}

SEXP buildRagraph(Agraph_t *g) {
    SEXP graphRef, klass, obj;

    PROTECT(graphRef = R_MakeExternalPtr(g,Rgraphviz_graph_type_tag,
				 R_NilValue));
    R_RegisterCFinalizer(graphRef, (R_CFinalizer_t)Rgraphviz_fin);

    klass = MAKE_CLASS("Ragraph");
    PROTECT(obj = NEW_OBJECT(klass));
    
    SET_SLOT(obj, Rf_install("agraph"), graphRef);
    SET_SLOT(obj, Rf_install("laidout"), R_scalarLogical(FALSE));
    SET_SLOT(obj, Rf_install("numEdges"), R_scalarInteger(agnedges(g)));

    UNPROTECT(2);

    return(obj);
}


SEXP getBoundBox(Agraph_t *g) {
    /* Determine the graphviz determiend bounding box and */
    /* assign it to the appropriate Ragraph structure */
    SEXP bbClass, xyClass, curBB, LLXY, URXY;

    xyClass = MAKE_CLASS("xyPoint");
    bbClass = MAKE_CLASS("boundingBox");

    PROTECT(curBB = NEW_OBJECT(bbClass));
    PROTECT(LLXY = NEW_OBJECT(xyClass));
    PROTECT(URXY = NEW_OBJECT(xyClass));

    SET_SLOT(LLXY,Rf_install("x"),R_scalarInteger(g->u.bb.LL.x));
    SET_SLOT(LLXY,Rf_install("y"),R_scalarInteger(g->u.bb.LL.y));
    SET_SLOT(URXY,Rf_install("x"),R_scalarInteger(g->u.bb.UR.x));
    SET_SLOT(URXY,Rf_install("y"),R_scalarInteger(g->u.bb.UR.y));

    SET_SLOT(curBB,Rf_install("botLeft"), LLXY);
    SET_SLOT(curBB,Rf_install("upRight"), URXY);

    UNPROTECT(3);
    return(curBB);
}

SEXP getNodeLayouts(Agraph_t *g) {
    Agnode_t *node;
    SEXP outLst, nlClass, xyClass, curXY, curNL;
    SEXP curLab, labClass;
    int i, nodes;
    char *tmpString;

    if (g == NULL)
	error("getNodeLayouts passed a NULL graph");

    nlClass = MAKE_CLASS("AgNode");
    xyClass = MAKE_CLASS("xyPoint");
    labClass = MAKE_CLASS("AgTextLabel");

    /* tmpString is used to convert a char to a char* w/ labels */
    tmpString = (char *)R_alloc(2, sizeof(char));
    if (tmpString == NULL)
	error("Allocation error in getNodeLayouts");

    nodes = agnnodes(g);
    node = agfstnode(g);

    PROTECT(outLst = allocVector(VECSXP, nodes));

    for (i = 0; i < nodes; i++) {	
	PROTECT(curNL = NEW_OBJECT(nlClass));
	PROTECT(curXY = NEW_OBJECT(xyClass));
	SET_SLOT(curXY,Rf_install("x"),R_scalarInteger(node->u.coord.x));
	SET_SLOT(curXY,Rf_install("y"),R_scalarInteger(node->u.coord.y));
	SET_SLOT(curNL,Rf_install("center"),curXY);
	SET_SLOT(curNL,Rf_install("height"),R_scalarInteger(node->u.ht));
	SET_SLOT(curNL,Rf_install("rWidth"),R_scalarInteger(node->u.rw));
	SET_SLOT(curNL,Rf_install("lWidth"),R_scalarInteger(node->u.lw));
	SET_SLOT(curNL,Rf_install("name"), R_scalarString(node->name));

	SET_SLOT(curNL, Rf_install("color"), 
		 R_scalarString(agget(node, "color")));
	SET_SLOT(curNL, Rf_install("fillcolor"),
		 R_scalarString(agget(node, "fillcolor")));
	SET_SLOT(curNL, Rf_install("shape"),
		 R_scalarString(agget(node, "shape")));
	SET_SLOT(curNL, Rf_install("style"),
		 R_scalarString(agget(node, "style")));


	PROTECT(curLab = NEW_OBJECT(labClass));
	if (node->u.label->u.txt.line != NULL) {
	    SET_SLOT(curLab, Rf_install("labelText"),
		     R_scalarString(node->u.label->u.txt.line->str));
	    snprintf(tmpString, 2, "%c",node->u.label->u.txt.line->just);
	    SET_SLOT(curLab, Rf_install("labelJust"),
		     R_scalarString(tmpString));
	    
	    SET_SLOT(curLab, Rf_install("labelWidth"),
		     R_scalarInteger(node->u.label->u.txt.line->width));
	    
	    /* Get the X/Y location of the label */
	    PROTECT(curXY = NEW_OBJECT(xyClass));
	    SET_SLOT(curXY, Rf_install("x"),
		     R_scalarInteger(node->u.label->p.x));
	    SET_SLOT(curXY, Rf_install("y"),
		     R_scalarInteger(node->u.label->p.y));
	    SET_SLOT(curLab, Rf_install("labelLoc"), curXY);
	    UNPROTECT(1);
	    
	    SET_SLOT(curLab, Rf_install("labelColor"),
		     R_scalarString(node->u.label->fontcolor));
	    
	    SET_SLOT(curLab, Rf_install("labelFontsize"),
 		     R_scalarReal(node->u.label->fontsize));
    
	}

	SET_SLOT(curNL, Rf_install("txtLabel"), curLab);
	
	SET_ELEMENT(outLst, i, curNL);
	node = agnxtnode(g,node);
	    
	UNPROTECT(3);
    }
    UNPROTECT(1);
    return(outLst);
}

SEXP getEdgeLocs(Agraph_t *g, int numEdges) {
    SEXP outList, curCP, curEP, pntList, pntSet, curXY, curLab;
    SEXP epClass, cpClass, xyClass, labClass;
    Agnode_t *node, *head;
    Agedge_t *edge;
    char *tmpString;
    bezier bez;
    int nodes;
    int i,k,l,pntLstEl;
    int curEle = 0;

    epClass = MAKE_CLASS("AgEdge");
    cpClass = MAKE_CLASS("BezierCurve");
    xyClass = MAKE_CLASS("xyPoint");
    labClass = MAKE_CLASS("AgTextLabel");

    /* tmpString is used to convert a char to a char* w/ labels */
    tmpString = (char *)R_alloc(2, sizeof(char));
    if (tmpString == NULL)
	error("Allocation error in getEdgeLocs");

    PROTECT(outList = allocVector(VECSXP, numEdges));

    nodes = agnnodes(g);
    node = agfstnode(g);

    for (i = 0; i < nodes; i++) {
	edge = agfstout(g, node);
	while (edge != NULL) {
	    PROTECT(curEP = NEW_OBJECT(epClass));
	    bez = edge->u.spl->list[0];
	    PROTECT(pntList = allocVector(VECSXP, 
					  ((bez.size-1)/3)));
	    pntLstEl = 0;

	    /* There are really (bez.size-1)/3 sets of control */
	    /* points, with the first set containing teh first 4 */
	    /* points, and then every other set starting with the */
	    /* last point from the previous set and then the next 3 */
	    for (k = 1; k < bez.size; k += 3) {
		PROTECT(curCP = NEW_OBJECT(cpClass));
		PROTECT(pntSet = allocVector(VECSXP, 4));
		for (l = -1; l < 3; l++) {
		    PROTECT(curXY = NEW_OBJECT(xyClass));
		    SET_SLOT(curXY, Rf_install("x"), 
			     R_scalarInteger(bez.list[k+l].x));
		    SET_SLOT(curXY, Rf_install("y"), 
			     R_scalarInteger(bez.list[k+l].y));
		    SET_ELEMENT(pntSet, l+1, curXY);
		    UNPROTECT(1);
		}
		SET_SLOT(curCP, Rf_install("cPoints"), pntSet);
		SET_ELEMENT(pntList, pntLstEl++, curCP);
		UNPROTECT(2);
	    }	    
	    SET_SLOT(curEP, Rf_install("splines"), pntList);
	    /* get the sp and ep */
	    PROTECT(curXY = NEW_OBJECT(xyClass));
	    SET_SLOT(curXY, Rf_install("x"),
		     R_scalarInteger(bez.sp.x));
	    SET_SLOT(curXY, Rf_install("y"),
		     R_scalarInteger(bez.sp.y));
	    SET_SLOT(curEP, Rf_install("sp"), curXY);
	    UNPROTECT(1);
	    PROTECT(curXY = NEW_OBJECT(xyClass));
	    SET_SLOT(curXY, Rf_install("x"),
		     R_scalarInteger(bez.ep.x));
	    SET_SLOT(curXY, Rf_install("y"),
		     R_scalarInteger(bez.ep.y));
	    SET_SLOT(curEP, Rf_install("ep"), curXY);
	    UNPROTECT(1);	    

	    SET_SLOT(curEP, Rf_install("tail"), 
		     R_scalarString(node->name));
	    head = edge->head;
	    SET_SLOT(curEP, Rf_install("head"),
		     R_scalarString(head->name));

	    SET_SLOT(curEP, Rf_install("arrowhead"),
		     R_scalarString(agget(edge, "arrowhead")));
	    SET_SLOT(curEP, Rf_install("arrowtail"),
		     R_scalarString(agget(edge, "arrowtail")));
	    SET_SLOT(curEP, Rf_install("arrowsize"),
		     R_scalarString(agget(edge, "arrowsize")));

	    SET_SLOT(curEP, Rf_install("color"), 
		     R_scalarString(agget(edge, "color")));

	    /* Get the label information */
	    if (edge->u.label != NULL) {
		PROTECT(curLab = NEW_OBJECT(labClass));
		SET_SLOT(curLab, Rf_install("labelText"),
			 R_scalarString(edge->u.label->u.txt.line->str));
		/* Get the X/Y location of the label */
		PROTECT(curXY = NEW_OBJECT(xyClass));
		SET_SLOT(curXY, Rf_install("x"),
			 R_scalarInteger(edge->u.label->p.x));
		SET_SLOT(curXY, Rf_install("y"),
			 R_scalarInteger(edge->u.label->p.y));
		SET_SLOT(curLab, Rf_install("labelLoc"), curXY);
		UNPROTECT(1);
			 
		snprintf(tmpString, 2, "%c",edge->u.label->u.txt.line->just);
		SET_SLOT(curLab, Rf_install("labelJust"),
			 R_scalarString(tmpString));

		SET_SLOT(curLab, Rf_install("labelWidth"),
			 R_scalarInteger(edge->u.label->u.txt.line->width));

		SET_SLOT(curLab, Rf_install("labelColor"),
			 R_scalarString(node->u.label->fontcolor));

		SET_SLOT(curLab, Rf_install("labelFontsize"),
			 R_scalarReal(edge->u.label->fontsize));

		SET_SLOT(curEP, Rf_install("txtLabel"), curLab);
		UNPROTECT(1);
	    }

	    SET_ELEMENT(outList, curEle++, curEP);
	    UNPROTECT(2);
	    edge = agnxtout(g, edge);
	}
	node = agnxtnode(g, node);
    }
    UNPROTECT(1);

    return(outList);
}

SEXP Rgraphviz_graphvizVersion(void) {
    return(R_scalarString(Info[1]));
}

Agraph_t *setDefaultAttrs(Agraph_t *g, SEXP attrs) {
    /* While attributes have default values already,  */
    /* if we want to dynamically set them, we need */
    /* to have defined defaults manually */
    int i;
    SEXP attrNames, elmt;

    /* Now set user defined attributes */
    /* Set the graph level attributes */
    PROTECT(elmt = getListElement(attrs, "graph"));
    /* Now elmt is a list of attributes to set */
    PROTECT(attrNames = getAttrib(elmt, R_NamesSymbol));
    for (i = 0; i < length(elmt); i++) {
	agraphattr(g, CHAR(STRING_ELT(attrNames,i)), STR(VECTOR_ELT(elmt,i)));
    }
    
    UNPROTECT(2);

    /* Now do node-wide */
    PROTECT(elmt = getListElement(attrs, "node"));
    PROTECT(attrNames = getAttrib(elmt, R_NamesSymbol));
    for (i = 0; i < length(elmt); i++) {
	agnodeattr(g, CHAR(STRING_ELT(attrNames,i)), STR(VECTOR_ELT(elmt,i)));
    }
    UNPROTECT(2);

    /* Lastly do edge-wide */
    PROTECT(elmt = getListElement(attrs, "edge"));
    PROTECT(attrNames = getAttrib(elmt, R_NamesSymbol));
    for (i = 0; i < length(elmt); i++) {
	agedgeattr(g, CHAR(STRING_ELT(attrNames,i)),
		   STR(VECTOR_ELT(elmt,i)));
   }
    UNPROTECT(2);

    return(g);
}

