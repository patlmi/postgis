/**********************************************************************
 * $Id$
 *
 * PostGIS - Spatial Types for PostgreSQL
 * http://postgis.refractions.net
 * 
 * Copyright 2006 Corporacion Autonoma Regional de Santander 
 *                Eduin Carrillo <yecarrillo@cas.gov.co>
 * Copyright 2010 Paul Ramsey <pramsey@cleverelephant.ca>
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of hte GNU General Public Licence. See the COPYING file.
 *
 **********************************************************************/

#include "liblwgeom_internal.h"
#include <math.h>	/* fabs */

static int lwgeom_to_kml2_sb(const LWGEOM *geom, int precision, const char *prefix, stringbuffer_t *sb);
static int lwpoint_to_kml2_sb(const LWPOINT *point, int precision, const char *prefix, stringbuffer_t *sb);
static int lwline_to_kml2_sb(const LWLINE *line, int precision, const char *prefix, stringbuffer_t *sb);
static int lwpoly_to_kml2_sb(const LWPOLY *poly, int precision, const char *prefix, stringbuffer_t *sb);
static int lwcollection_to_kml2_sb(const LWCOLLECTION *col, int precision, const char *prefix, stringbuffer_t *sb);
static int ptarray_to_kml2_sb(const POINTARRAY *pa, int precision, stringbuffer_t *sb);

/*
* KML 2.2.0
*/

/* takes a GEOMETRY and returns a KML representation */
char*
lwgeom_to_kml2(const LWGEOM *geom, int precision, const char *prefix)
{
	stringbuffer_t *sb;
	int rv;
	char *kml;

	/* Can't do anything with empty */
	if( lwgeom_is_empty(geom) )
		return NULL;

	sb = stringbuffer_create();
	rv = lwgeom_to_kml2_sb(geom, precision, prefix, sb);
	
	if ( rv == LW_FAILURE )
	{
		stringbuffer_destroy(sb);
		return NULL;
	}
	
	kml = stringbuffer_getstringcopy(sb);
	stringbuffer_destroy(sb);
	
	return kml;
}

static int 
lwgeom_to_kml2_sb(const LWGEOM *geom, int precision, const char *prefix, stringbuffer_t *sb)
{
	switch (geom->type)
	{
	case POINTTYPE:
		return lwpoint_to_kml2_sb((LWPOINT*)geom, precision, prefix, sb);

	case LINETYPE:
		return lwline_to_kml2_sb((LWLINE*)geom, precision, prefix, sb);

	case POLYGONTYPE:
		return lwpoly_to_kml2_sb((LWPOLY*)geom, precision, prefix, sb);

	case MULTIPOINTTYPE:
	case MULTILINETYPE:
	case MULTIPOLYGONTYPE:
	case COLLECTIONTYPE:
		return lwcollection_to_kml2_sb((LWCOLLECTION*)geom, precision, prefix, sb);

	default:
		lwerror("lwgeom_to_kml2_sb: '%s' geometry type not supported", lwtype_name(geom->type));
		return LW_FAILURE;
	}
}

static int 
ptarray_to_kml2_sb(const POINTARRAY *pa, int precision, stringbuffer_t *sb)
{
	int i;
	int rv;
	POINT4D pt;
	
	for ( i = 0; i < pa->npoints; i++ )
	{
		getPoint4d_p(pa, i, &pt);
		if ( i )
			stringbuffer_append(sb, " ");
		if ( FLAGS_GET_Z(pa->flags) )
			rv = stringbuffer_aprintf(sb, "%.*g,%.*g,%.*g", precision, pt.x, pt.y, pt.z);
		else
			rv = stringbuffer_aprintf(sb, "%.*g,%.*g,%.*g", precision, pt.x, pt.y);
		if ( rv < 0 )
			return LW_FAILURE;
	}
	return LW_SUCCESS;
}


static int 
lwpoint_to_kml2_sb(const LWPOINT *point, int precision, const char *prefix, stringbuffer_t *sb)
{
	/* Open point */
	if ( stringbuffer_aprintf(sb, "<%sPoint><%scoordinates>", prefix, prefix) < 0 ) return LW_FAILURE;
	/* Coordinate array */
	if ( ptarray_to_kml2_sb(point->point, precision, sb) == LW_FAILURE ) return LW_FAILURE;
	/* Close point */
	if ( stringbuffer_aprintf(sb, "</%scoordinates></%sPoint>", prefix, prefix) < 0 ) return LW_FAILURE;
	return LW_SUCCESS;
}

static int 
lwline_to_kml2_sb(const LWLINE *line, int precision, const char *prefix, stringbuffer_t *sb)
{
	/* Open linestring */
	if ( stringbuffer_aprintf(sb, "<%sLineString><%scoordinates>", prefix, prefix) < 0 ) return LW_FAILURE;
	/* Coordinate array */
	if ( ptarray_to_kml2_sb(line->points, precision, sb) == LW_FAILURE ) return LW_FAILURE;
	/* Close linestring */
	if ( stringbuffer_aprintf(sb, "</%scoordinates></%sLineString>", prefix, prefix) < 0 ) return LW_FAILURE;
	
	return LW_SUCCESS;
}

static int 
lwpoly_to_kml2_sb(const LWPOLY *poly, int precision, const char *prefix, stringbuffer_t *sb)
{
	int i, rv;
	
	/* Open polygon */
	if ( stringbuffer_aprintf(sb, "<%sPolygon>", prefix) < 0 ) return LW_FAILURE;
	for ( i = 0; i < poly->nrings; i++ )
	{
		/* Inner or outer ring opening tags */
		if( i )
			rv = stringbuffer_aprintf(sb, "<%sinnerBoundaryIs><%sLinearRing><%scoordinates>", prefix, prefix, prefix);
		else
			rv = stringbuffer_aprintf(sb, "<%souterBoundaryIs><%sLinearRing><%scoordinates>", prefix, prefix, prefix);		
		if ( rv < 0 ) return LW_FAILURE;
		
		/* Coordinate array */
		if ( ptarray_to_kml2_sb(poly->rings[i], precision, sb) == LW_FAILURE ) return LW_FAILURE;
		
		/* Inner or outer ring closing tags */
		if( i )
			rv = stringbuffer_aprintf(sb, "</%scoordinates></%sLinearRing></%sinnerBoundaryIs>", prefix, prefix, prefix);
		else
			rv = stringbuffer_aprintf(sb, "</%scoordinates></%sLinearRing></%souterBoundaryIs>", prefix, prefix, prefix);		
		if ( rv < 0 ) return LW_FAILURE;
	}
	/* Close polygon */
	if ( stringbuffer_aprintf(sb, "</%sPolygon>", prefix) < 0 ) return LW_FAILURE;

	return LW_SUCCESS;
}

static int 
lwcollection_to_kml2_sb(const LWCOLLECTION *col, int precision, const char *prefix, stringbuffer_t *sb)
{
	int i, rv;
	
	/* Open geometry */
	if ( stringbuffer_aprintf(sb, "<%sMultiGeometry>", prefix) < 0 ) return LW_FAILURE;
	for ( i = 0; i < col->ngeoms; i++ )
	{
		rv = lwgeom_to_kml2_sb(col->geoms[i], precision, prefix, sb);
		if ( rv == LW_FAILURE ) return LW_FAILURE;		
	}
	/* Close geometry */
	if ( stringbuffer_aprintf(sb, "</%sMultiGeometry>", prefix) < 0 ) return LW_FAILURE;

	return LW_SUCCESS;
}
