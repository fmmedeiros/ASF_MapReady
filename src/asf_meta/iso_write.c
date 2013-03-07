#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "asf_iso_meta.h"
#include "asf_nan.h"

static void dateTime2str(iso_dateTime timeUTC, char *str)
{
  int year = timeUTC.year;
  int month = timeUTC.month;
  int day = timeUTC.day;
  int hour = timeUTC.hour;
  int min = timeUTC.min;
  double sec = timeUTC.second;

  // UTC time stamp: 2008-03-13T22:19:55.140975
  if (year < 0 || month < 0 || day < 0 || hour < 0 || min < 0 || sec < 0.0)
    strcpy(str, "1900-01-01T00:00:00.000000");
  else
    sprintf(str, "%4d-%02d-%02dT%02d:%02d:%09.6f", 
	    year, month, day, hour, min, sec);
}

void double2str(double value, int decimals, char *str)
{
  if (ISNAN(value))
    strcpy(str, "NaN");
  else if (decimals == 0)
    sprintf(str, "%g", value);
  else {
    char format[10];
    sprintf(format, "%%.%dlf", decimals);
    sprintf(str, format, value);
  }
}

void iso_meta_write(iso_meta *iso, const char *outFile)
{
  unsigned long ii, kk;
  char *str;
  
  // Set up 
  xmlDoc *doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNode *root = xmlNewNode(NULL, BAD_CAST "level1Product");
  xmlNs *ns = xmlNewNs(root, 
		       BAD_CAST "http://www.w3.org/2001/XMLSchema-instance",
		       BAD_CAST "xsi");
  //xmlNewNsProp(root, ns, BAD_CAST "noNamespaceSchemaLocation",
  //	       BAD_CAST "iso_meta.xsd");
  xmlNewNsProp(root, ns, BAD_CAST "schemaLocation", BAD_CAST "iso_meta.xsd");
  xmlDocSetRootElement(doc, root);
  
  // Generate the tree from iso structure
  xmlNodePtr unit, node3, node2, node, parent, section;
  str = (char *) MALLOC(sizeof(char)*1024);

  // General Header
  iso_generalHeader *header = iso->generalHeader;
  section = xmlNewChild(root, NULL, BAD_CAST "generalHeader", NULL);
  xmlNewProp(section, BAD_CAST "fileName", BAD_CAST outFile);
  xmlNewProp(section, BAD_CAST "fileVersion", BAD_CAST "1.0");
  xmlNewProp(section, BAD_CAST "status", BAD_CAST "PRELIMINARY");
  xmlNewChild(section, NULL, BAD_CAST "itemName", BAD_CAST header->itemName);
  xmlNewChild(section, NULL, BAD_CAST "mission", BAD_CAST header->mission);
  xmlNewChild(section, NULL, BAD_CAST "source", BAD_CAST header->source);
  xmlNewChild(section, NULL, BAD_CAST "destination", 
	      BAD_CAST header->destination);
  xmlNewChild(section, NULL, BAD_CAST "generationSystem", 
	      BAD_CAST header->generationSystem);
  dateTime2str(header->generationTime, str);
  xmlNewChild(section, NULL, BAD_CAST "generationTime", BAD_CAST str);
  if (header->referenceDocument)
    xmlNewChild(section, NULL, BAD_CAST "referenceDocument", 
		BAD_CAST header->referenceDocument);
  if (header->revision)
    xmlNewChild(section, NULL, BAD_CAST "revision", BAD_CAST header->revision);
  if (header->revisionComment)
    xmlNewChild(section, NULL, BAD_CAST "revisionComment", 
		BAD_CAST header->revisionComment);
  
  // Product Components
  iso_productComponents *comps = iso->productComponents;
  section = xmlNewChild(root, NULL, BAD_CAST "productComponents", NULL);
  if (comps->annotation) {
    parent = xmlNewChild(section, NULL, BAD_CAST "annotation", NULL);
    for (ii=0; ii<comps->numAnnotations; ii++) {
      if (comps->annotation[ii].type == MAIN_TYPE)
	strcpy(str, "MAIN");
      else if (comps->annotation[ii].type == GEOREF_TYPE)
	strcpy(str, "GEOREF");
      else if (comps->annotation[ii].type == GEOCODE_TYPE)
	strcpy(str, "GEOCODE");
      else if (comps->annotation[ii].type == OTHER_TYPE)
	strcpy(str, "OTHER");
      else if (comps->annotation[ii].type == UNDEF_TYPE)
	strcpy(str, "UNDEFINED");
      xmlNewChild(parent, NULL, BAD_CAST "type", BAD_CAST str);
      node = xmlNewChild(parent, NULL, BAD_CAST "file", NULL);
      parent = node;
      node = xmlNewChild(parent, NULL, BAD_CAST "location", NULL);
      xmlNewChild(node, NULL, BAD_CAST "host", 
		  BAD_CAST comps->annotation[ii].file.host);
      xmlNewChild(node, NULL, BAD_CAST "path", 
		  BAD_CAST comps->annotation[ii].file.path);
      xmlNewChild(node, NULL, BAD_CAST "filename", 
		  BAD_CAST comps->annotation[ii].file.name);
      sprintf(str, "%ld", comps->annotation[ii].file.size);
      xmlNewChild(parent, NULL, BAD_CAST "size", BAD_CAST str);
    }
  }
  if (comps->imageData) {
    for (ii=0; ii<comps->numLayers; ii++) {
      parent = xmlNewChild(section, NULL, BAD_CAST "imageData", NULL);
      sprintf(str, "%ld", ii+1);
      xmlNewProp(parent, BAD_CAST "layerIndex", BAD_CAST str);
      if (comps->imageData[ii].polLayer == HH_POL)
	strcpy(str, "HH");
      else if (comps->imageData[ii].polLayer == HV_POL)
	strcpy(str, "HV");
      else if (comps->imageData[ii].polLayer == VH_POL)
	strcpy(str, "VH");
      else if (comps->imageData[ii].polLayer == VV_POL)
	strcpy(str, "VV");
      else if (comps->imageData[ii].polLayer == UNDEF_POL_LAYER)
	strcpy(str, "UNDEFINED");
      xmlNewChild(parent, NULL, BAD_CAST "polLayer", BAD_CAST str);
      node = xmlNewChild(parent, NULL, BAD_CAST "file", NULL);
      node2 = xmlNewChild(node, NULL, BAD_CAST "location", NULL);
      xmlNewChild(node2, NULL, BAD_CAST "host", 
		  BAD_CAST comps->imageData[ii].file.host);
      xmlNewChild(node2, NULL, BAD_CAST "path",
		  BAD_CAST comps->imageData[ii].file.path);
      xmlNewChild(node2, NULL, BAD_CAST "filename",
		  BAD_CAST comps->imageData[ii].file.name);
      sprintf(str, "%ld", comps->imageData[ii].file.size);
      xmlNewChild(node, NULL, BAD_CAST "size", BAD_CAST str);
    }
  }
  if (comps->auxRasterFiles) {
    // FIXME: fill as needed
  }
  if (comps->quicklooks) {
    for (ii=0; ii<comps->numLayers; ii++) {
      parent = xmlNewChild(section, NULL, BAD_CAST "quicklooks", NULL);
      sprintf(str, "%ld", ii+1);
      xmlNewProp(parent, BAD_CAST "layerIndex", BAD_CAST str);
      if (comps->quicklooks[ii].polLayer == HH_POL)
	strcpy(str, "HH");
      else if (comps->quicklooks[ii].polLayer == HV_POL)
	strcpy(str, "HV");
      else if (comps->quicklooks[ii].polLayer == VH_POL)
	strcpy(str, "VH");
      else if (comps->quicklooks[ii].polLayer == VV_POL)
	strcpy(str, "VV");
      else if (comps->quicklooks[ii].polLayer == UNDEF_POL_LAYER)
	strcpy(str, "UNDEFINED");
      xmlNewChild(parent, NULL, BAD_CAST "polLayer", BAD_CAST str);
      node = xmlNewChild(parent, NULL, BAD_CAST "file", NULL);
      node2 = xmlNewChild(node, NULL, BAD_CAST "location", NULL);
      xmlNewChild(node2, NULL, BAD_CAST "host", 
		  BAD_CAST comps->quicklooks[ii].file.host);
      xmlNewChild(node2, NULL, BAD_CAST "path",
		  BAD_CAST comps->quicklooks[ii].file.path);
      xmlNewChild(node2, NULL, BAD_CAST "filename",
		  BAD_CAST comps->quicklooks[ii].file.name);
      sprintf(str, "%ld", comps->quicklooks[ii].file.size);
      xmlNewChild(node, NULL, BAD_CAST "size", BAD_CAST str);
    }
  }

  /*
    if (comps->compositeQuicklook) {
    parent = xmlNewChild(section, NULL, BAD_CAST "compositeQuicklook", NULL);
    node = xmlNewChild(parent, NULL, BAD_CAST "file", NULL);
    parent = node;
    node = xmlNewChild(parent, NULL, BAD_CAST "location", NULL);
    xmlNewChild(node, NULL, BAD_CAST "host", 
    BAD_CAST comps->compositeQuicklook.host);
    xmlNewChild(node, NULL, BAD_CAST "path", 
    BAD_CAST comps->compositeQuicklook.path);
    xmlNewChild(node, NULL, BAD_CAST "filename", 
    BAD_CAST comps->compositeQuicklook.name);
    xmlNewChild(parent, NULL, BAD_CAST "size", 
    ldStr(comps->compositeQuicklook.size));
    }
  */
  parent = xmlNewChild(section, NULL, BAD_CAST "browseImage", NULL);
  node = xmlNewChild(parent, NULL, BAD_CAST "file", NULL);
  parent = node;
  node = xmlNewChild(parent, NULL, BAD_CAST "location", NULL);
  xmlNewChild(node, NULL, BAD_CAST "host", BAD_CAST comps->browseImage.host);
  xmlNewChild(node, NULL, BAD_CAST "path", BAD_CAST comps->browseImage.path);
  xmlNewChild(node, NULL, BAD_CAST "filename", 
	      BAD_CAST comps->browseImage.name);
  sprintf(str, "%ld", comps->browseImage.size);
  xmlNewChild(parent, NULL, BAD_CAST "size", BAD_CAST str);
  parent = xmlNewChild(section, NULL, BAD_CAST "mapPlot", NULL);
  node = xmlNewChild(parent, NULL, BAD_CAST "file", NULL);
  parent = node;
  node = xmlNewChild(parent, NULL, BAD_CAST "location", NULL);
  xmlNewChild(node, NULL, BAD_CAST "host", BAD_CAST comps->mapPlot.host);
  xmlNewChild(node, NULL, BAD_CAST "path", BAD_CAST comps->mapPlot.path);
  xmlNewChild(node, NULL, BAD_CAST "filename", BAD_CAST comps->mapPlot.name);
  sprintf(str, "%ld", comps->mapPlot.size);
  xmlNewChild(parent, NULL, BAD_CAST "size", BAD_CAST str);
  
  // Product Info
  iso_productInfo *info = iso->productInfo;
  section = xmlNewChild(root, NULL, BAD_CAST "productInfo", NULL);
  parent = xmlNewChild(section, NULL, BAD_CAST "generationInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "logicalProductID", 
	      BAD_CAST info->logicalProductID);
  xmlNewChild(parent, NULL, BAD_CAST "receivingStation", 
	      BAD_CAST info->receivingStation);
  xmlNewChild(parent, NULL, BAD_CAST "level0ProcessingFacility", 
	      BAD_CAST info->level0ProcessingFacility);
  xmlNewChild(parent, NULL, BAD_CAST "level1ProcessingFacility",
	      BAD_CAST info->level1ProcessingFacility);
  if (info->groundOperationsType == OPERATIONAL)
    strcpy(str, "OPERATIONAL");
  else if (info->groundOperationsType == PREOPERATIONAL)
    strcpy(str, "PRE-OPERATIONAL");
  else if (info->groundOperationsType == INSTRUMENT)
    strcpy(str, "INSTRUMENT");
  else if (info->groundOperationsType == TEST_OPS)
    strcpy(str, "TEST");
  else if (info->groundOperationsType == UNDEF_OPS)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "groundOperationsType", BAD_CAST str);
  xmlNewChild(parent, NULL, BAD_CAST "deliveryInfo", 
	      BAD_CAST info->deliveryInfo);
  xmlNewChild(parent, NULL, BAD_CAST "copyrightInfo", 
	      BAD_CAST info->copyrightInfo);
  node = xmlNewChild(parent, NULL, BAD_CAST "qualityInfo", NULL);
  if (info->qualityInspection == AUTO_APPROVED)
    strcpy(str, "AUTO APPROVED");
  else if (info->qualityInspection == OPERATOR_APPROVED)
    strcpy(str, "OPERATOR APPROVED");
  else if (info->qualityInspection == NOT_APPROVED)
    strcpy(str, "NOT APPROVED");
  else if (info->qualityInspection == UNDEF_QUALITY)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "qualityInspection", BAD_CAST str);
  if (info->qualityRemark)
    xmlNewChild(node, NULL, BAD_CAST "qualityRemark", 
		BAD_CAST info->qualityRemark);
  
  parent = xmlNewChild(section, NULL, BAD_CAST "missionInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "mission", BAD_CAST info->mission);
  sprintf(str, "%d", info->orbitPhase);
  xmlNewChild(parent, NULL, BAD_CAST "orbitPhase", BAD_CAST str);
  sprintf(str, "%d", info->orbitCycle);
  xmlNewChild(parent, NULL, BAD_CAST "orbitCycle", BAD_CAST str);
  sprintf(str, "%d", info->absOrbit);
  xmlNewChild(parent, NULL, BAD_CAST "absOrbit", BAD_CAST str);
  sprintf(str, "%d", info->relOrbit);
  xmlNewChild(parent, NULL, BAD_CAST "relOrbit", BAD_CAST str);
  sprintf(str, "%d", info->numOrbitsInCycle);
  xmlNewChild(parent, NULL, BAD_CAST "numOrbitsInCycle", BAD_CAST str);
  if (info->orbitDirection == ASCENDING)
    strcpy(str, "ASCENDING");
  else if (info->orbitDirection == DESCENDING)
    strcpy(str, "DESCENDING");
  else if (info->orbitDirection == UNDEF_ORBIT)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "orbitDirection", BAD_CAST str);
  
  parent = xmlNewChild(section, NULL, BAD_CAST "acquisitionInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "sensor", BAD_CAST info->sensor);
  if (info->imageMode == FINE_BEAM)
    strcpy(str, "FINE BEAM");
  else if (info->imageMode == STANDARD_BEAM)
    strcpy(str, "STANDARD BEAM");
  else if (info->imageMode == STRIPMAP_IMAGE)
    strcpy(str, "STRIPMAP");
  else if (info->imageMode == SCANSAR_IMAGE)
    strcpy(str, "SCANSAR");
  else if (info->imageMode == SPOTLIGHT_IMAGE)
    strcpy(str, "SPOTLIGHT");
  else if (info->imageMode == UNDEF_IMAGE_MODE)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "imagingMode", BAD_CAST str);
  if (info->lookDirection == RIGHT_LOOK)
    strcpy(str, "RIGHT");
  else if (info->lookDirection == LEFT_LOOK)
    strcpy(str, "LEFT");
  else if (info->lookDirection == UNDEF_LOOK)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "lookDirection", BAD_CAST str);
  if (info->polarizationMode == SINGLE_POL)
    strcpy(str, "SINGLE");
  else if (info->polarizationMode == DUAL_POL)
    strcpy(str, "DUAL");
  else if (info->polarizationMode == QUAD_POL)
    strcpy(str, "QUAD");
  else if (info->polarizationMode == UNDEF_POL_MODE)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "polarizationMode", BAD_CAST str);
  node = xmlNewChild(parent, NULL, BAD_CAST "polarizationList", NULL);
  for (ii=0; ii<comps->numLayers; ii++) {
    if (info->polLayer[ii] == HH_POL)
      strcpy(str, "HH");
    else if (info->polLayer[ii] == HV_POL)
      strcpy(str, "HV");
    else if (info->polLayer[ii] == VH_POL)
      strcpy(str, "VH");
    else if (info->polLayer[ii] == VV_POL)
      strcpy(str, "VV");
    else if (info->polLayer[ii] == UNDEF_POL_LAYER)
      strcpy(str, "UNDEFINED");
    xmlNewChild(node, NULL, BAD_CAST "polLayer", BAD_CAST str);
  }
  xmlNewChild(parent, NULL, BAD_CAST "elevationBeamConfiguration", 
	      BAD_CAST info->elevationBeamConfiguration);
  node = xmlNewChild(parent, NULL, BAD_CAST "imagingModeSpecificInfo", NULL);
  if (info->imageMode == FINE_BEAM ||
      info->imageMode == STANDARD_BEAM ||
      info->imageMode == STRIPMAP_IMAGE) {
    if (info->imageMode == FINE_BEAM)
      parent = xmlNewChild(node, NULL, BAD_CAST "fineBeam", NULL);
    else if (info->imageMode == STANDARD_BEAM)
      parent = xmlNewChild(node, NULL, BAD_CAST "standardBeam", NULL);
    else if (info->imageMode == STRIPMAP_IMAGE)
      parent = xmlNewChild(node, NULL, BAD_CAST "stripMap", NULL);
    xmlNewChild(parent, NULL, BAD_CAST "azimuthBeamID", 
		BAD_CAST info->azimuthBeamID);
  }
  else if (info->imageMode == SCANSAR_IMAGE) {
    parent = xmlNewChild(node, NULL, BAD_CAST "scanSAR", NULL);
    sprintf(str, "%d", info->numberOfBeams);
    xmlNewChild(parent, NULL, BAD_CAST "numberOfBeams", BAD_CAST str);
    xmlNodePtr beams = xmlNewChild(parent, NULL, BAD_CAST "beamList", NULL);
    for (ii=0; ii<info->numberOfBeams; ii++)
      xmlNewChild(beams, NULL, BAD_CAST "beamID", BAD_CAST info->beamID[ii]);
    xmlNewChild(parent, NULL, BAD_CAST "azimuthBeamID", 
		BAD_CAST info->azimuthBeamID);
    sprintf(str, "%d", info->numberOfBursts);
    xmlNewChild(parent, NULL, BAD_CAST "numberOfBursts", BAD_CAST str);
  }
  else if (info->imageMode == SPOTLIGHT_IMAGE) {
    parent = xmlNewChild(node, NULL, BAD_CAST "spotLight", NULL);
    sprintf(str, "%d", info->numberOfAzimuthBeams);
    xmlNewChild(parent, NULL, BAD_CAST "numberOfAzimuthBeams", BAD_CAST str);
    xmlNewChild(parent, NULL, BAD_CAST "azimuthBeamIDFirst", 
		BAD_CAST info->azimuthBeamIDFirst);
    xmlNewChild(parent, NULL, BAD_CAST "azimuthBeamIDLast", 
		BAD_CAST info->azimuthBeamIDLast);
    double2str(info->azimuthSteeringAngleFirst, 5, str);
    unit = xmlNewChild(parent, NULL, BAD_CAST "azimuthSteeringAngleFirst",
		       BAD_CAST str);
    xmlNewProp(node, BAD_CAST "units", BAD_CAST "degrees");
    double2str(info->azimuthSteeringAngleLast, 5, str);
    unit = xmlNewChild(parent, NULL, BAD_CAST "azimuthSteeringAngleLast",
		       BAD_CAST str);
    xmlNewProp(node, BAD_CAST "units", BAD_CAST "degrees");
  }
  
  parent = xmlNewChild(section, NULL, BAD_CAST "productVariantInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "productType", BAD_CAST info->productType);
  if (info->productVariant == SLC_PRODUCT)
    strcpy(str, "SLC");
  else if (info->productVariant == STD_PRODUCT)
    strcpy(str, "STD");
  else if (info->productVariant == TC_PRODUCT)
    strcpy(str, "TC");
  else if (info->productVariant == RTC_PRODUCT)
    strcpy(str, "RTC");
  else if (info->productVariant == GEO_PRODUCT)
    strcpy(str, "GEO");
  else if (info->productVariant == SSC_PRODUCT)
    strcpy(str, "SSC");
  else if (info->productVariant == MGD_PRODUCT)
    strcpy(str, "MGD");
  else if (info->productVariant == GEC_PRODUCT)
    strcpy(str, "GEC");
  else if (info->productVariant == EEC_PRODUCT)
    strcpy(str, "EEC");
  else if (info->productVariant == UNDEF_PRODUCT)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "productVariant", BAD_CAST str);
  if (info->projection == SLANTRANGE_PROJ)
    strcpy(str, "SLANTRANGE");
  else if (info->projection == GROUNDRANGE_PROJ)
    strcpy(str, "GROUNDRANGE");
  else if (info->projection == MAP_PROJ)
    strcpy(str, "MAP");
  else if (info->projection == UNDEF_PROJ)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "projection", BAD_CAST str);
  if (info->projection == MAP_PROJ) {
    if (info->mapProjection == UTM_PROJ)
      strcpy(str, "UTM");
    else if (info->mapProjection == PS_PROJ)
      strcpy(str, "POLARSTEREOGRAPHIC");
    else if (info->mapProjection == GEOG_PROJ)
      strcpy(str, "GEOGRAPHIC");
    else if (info->mapProjection == UNDEF_MAP)
      strcpy(str, "UNDEFINED");
    xmlNewChild(parent, NULL, BAD_CAST "mapProjection", BAD_CAST str);
  }
  if (info->resolutionVariant == SE_RES)
    strcpy(str, "SE");
  else if (info->resolutionVariant == RE_RES)
    strcpy(str, "RE");
  else if (info->resolutionVariant == UNDEF_RES)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "resolutionVariant", BAD_CAST str);
  if (info->radiometricCorrection == CALIBRATED)
    strcpy(str, "CALIBRATED");
  else if (info->radiometricCorrection == RELCALIBRATED)
    strcpy(str, "RELATIVE CALIBRATED");
  else if (info->radiometricCorrection == NOTCALIBRATED)
    strcpy(str, "NOT CALIBRATED");
  else if (info->radiometricCorrection == UNDEF_CAL)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "radiometricCorrection", BAD_CAST str);
  
  parent = xmlNewChild(section, NULL, BAD_CAST "imageDataInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "pixelValueID", 
	      BAD_CAST info->pixelValueID);
  if (info->imageDataType == DETECTED_DATA_TYPE)
    strcpy(str, "DETECTED");
  else if (info->imageDataType == COMPLEX_DATA_TYPE)
    strcpy(str, "COMPLEX");
  else if (info->imageDataType == RAW_DATA_TYPE)
    strcpy(str, "RAW");
  else if (info->imageDataType == UNDEF_DATA_TYPE)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "imageDataType", BAD_CAST str);
  if (info->imageDataFormat == CEOS_DATA_FORMAT)
    strcpy(str, "CEOS");
  else if (info->imageDataFormat == GEOTIFF_DATA_FORMAT)
    strcpy(str, "GEOTIFF");
  else if (info->imageDataFormat == HDF5_DATA_FORMAT)
    strcpy(str, "HDF5");
  else if (info->imageDataFormat == COSAR_DATA_FORMAT)
    strcpy(str, "COSAR");
  else if (info->imageDataFormat == UNDEF_DATA_FORMAT)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "imageDataFormat", BAD_CAST str);
  sprintf(str, "%d", info->numberOfLayers);
  xmlNewChild(parent, NULL, BAD_CAST "numberOfLayers", BAD_CAST str);
  sprintf(str, "%d", info->imageDataDepth);
  xmlNewChild(parent, NULL, BAD_CAST "imageDataDepth", BAD_CAST str);
  if (info->imageStorageOrder == ROWBYROW)
    strcpy(str, "ROWBYROW");
  else if (info->imageStorageOrder == COLBYCOL)
    strcpy(str, "COLBYCOL");
  else if (info->imageStorageOrder == UNDEF_STORE)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "imageStoreOrder", BAD_CAST str);
  xmlNewChild(parent, NULL, BAD_CAST "rowContent", BAD_CAST info->rowContent);
  xmlNewChild(parent, NULL, BAD_CAST "columnContent", 
	      BAD_CAST info->columnContent);
  node = xmlNewChild(parent, NULL, BAD_CAST "imageRaster", NULL);
  sprintf(str, "%d", info->numberOfRows);
  xmlNewChild(node, NULL, BAD_CAST "numberOfRows", BAD_CAST str);
  sprintf(str, "%d", info->numberOfColumns);
  xmlNewChild(node, NULL, BAD_CAST "numberOfColumns", BAD_CAST str);
  sprintf(str, "%d", info->startRow);
  xmlNewChild(node, NULL, BAD_CAST "startRow", BAD_CAST str);
  sprintf(str, "%d", info->startColumn);
  xmlNewChild(node, NULL, BAD_CAST "startColumn", BAD_CAST str);
  double2str(info->rowScaling, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "rowScaling", BAD_CAST str);
  double2str(info->columnScaling, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "columnScaling", BAD_CAST str);
  double2str(info->rowSpacing, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "rowSpacing", BAD_CAST str);
  if (info->projection == MAP_PROJ)
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");  
  else
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  double2str(info->columnSpacing, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "columnSpacing", BAD_CAST str);
  if (info->projection == MAP_PROJ)
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");  
  else
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  double2str(info->groundRangeResolution, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "groundRangeResolution", 
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(info->azimuthResolution, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "azimuthResolution", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(info->azimuthLooks, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "azimuthLooks", BAD_CAST str);
  double2str(info->rangeLooks, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "rangeLooks", BAD_CAST str);
  
  parent = xmlNewChild(section, NULL, BAD_CAST "sceneInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "sceneID",  BAD_CAST info->sceneID);
  node = xmlNewChild(parent, NULL, BAD_CAST "start", NULL);
  dateTime2str(info->startTimeUTC, str);
  xmlNewChild(node, NULL, BAD_CAST "timeUTC", BAD_CAST str);
  /*
    unit = xmlNewChild(node, NULL, BAD_CAST "timeGPS", ldStr(info->startTimeGPS));
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
    unit = xmlNewChild(node, NULL, BAD_CAST "timeGPSFraction", 
    fStr(info->startTimeGPSFraction));
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  */
  node = xmlNewChild(parent, NULL, BAD_CAST "stop", NULL);
  dateTime2str(info->stopTimeUTC, str);
  xmlNewChild(node, NULL, BAD_CAST "timeUTC", BAD_CAST str);
  /*
    unit = xmlNewChild(node, NULL, BAD_CAST "timeGPS", ldStr(info->stopTimeGPS));
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
    unit = xmlNewChild(node, NULL, BAD_CAST "timeGPSFraction", 
    fStr(info->stopTimeGPSFraction));
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  */
  node = xmlNewChild(parent, NULL, BAD_CAST "rangeTime", NULL);
  double2str(info->rangeTimeFirstPixel, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "firstPixel", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  double2str(info->rangeTimeLastPixel, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "lastPixel", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  double2str(info->sceneAzimuthExtent, 3, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "sceneAzimuthExtent", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(info->sceneRangeExtent, 3, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "sceneRangeExtent", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  node = xmlNewChild(parent, NULL, BAD_CAST "sceneCenterCoord", NULL);
  sprintf(str, "%i", info->sceneCenterCoord.refRow);
  xmlNewChild(node, NULL, BAD_CAST "refRow", BAD_CAST str);
  sprintf(str, "%i", info->sceneCenterCoord.refColumn);
  xmlNewChild(node, NULL, BAD_CAST "refColumn", BAD_CAST str);
  double2str(info->sceneCenterCoord.lat, 5, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "lat", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  double2str(info->sceneCenterCoord.lon, 5, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "lon", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  dateTime2str(info->sceneCenterCoord.azimuthTimeUTC, str);
  xmlNewChild(node, NULL, BAD_CAST "azimuthTimeUTC", BAD_CAST str);
  double2str(info->sceneCenterCoord.rangeTime, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "rangeTime", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  double2str(info->sceneCenterCoord.incidenceAngle, 5, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "incidenceAngle", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  double2str(info->sceneAverageHeight, 3, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "sceneAverageHeight", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  for (ii=0; ii<4; ii++) {
    node = xmlNewChild(parent, NULL, BAD_CAST "sceneCornerCoord", NULL);
    sprintf(str, "%i", info->sceneCornerCoord[ii].refRow);
    xmlNewChild(node, NULL, BAD_CAST "refRow", BAD_CAST str);
    sprintf(str, "%i", info->sceneCornerCoord[ii].refColumn);
    xmlNewChild(node, NULL, BAD_CAST "refColumn",BAD_CAST str);
    double2str(info->sceneCornerCoord[ii].lat, 5, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "lat", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(info->sceneCornerCoord[ii].lon, 5, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "lon", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    dateTime2str(info->sceneCornerCoord[ii].azimuthTimeUTC, str);
    xmlNewChild(node, NULL, BAD_CAST "azimuthTimeUTC", BAD_CAST str);
    double2str(info->sceneCornerCoord[ii].rangeTime, 0, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "rangeTime", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
    double2str(info->sceneCornerCoord[ii].incidenceAngle, 5, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "incidenceAngle", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  }
  double2str(info->yaw, 5, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "yaw", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  double2str(info->pitch, 5, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "pitch", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  double2str(info->roll, 5, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "roll", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  double2str(info->headingAngle, 5, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "headingAngle", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  double2str(info->earthRadius, 3, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "earthRadius", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(info->satelliteHeight, 3, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "satelliteHeight", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  
  parent = xmlNewChild(section, NULL, BAD_CAST "previewInfo", NULL);
  node = xmlNewChild(parent, NULL, BAD_CAST "quicklooks", NULL);
  xmlNewChild(node, NULL, BAD_CAST "imageDataFormat", 
	      BAD_CAST info->quicklooks.imageDataFormat);
  sprintf(str, "%d", info->quicklooks.imageDataDepth);
  xmlNewChild(node, NULL, BAD_CAST "imageDataDepth", BAD_CAST str);
  node2 = xmlNewChild(node, NULL, BAD_CAST "imageRaster", NULL);
  sprintf(str, "%d", info->quicklooks.numberOfRows);
  xmlNewChild(node2, NULL, BAD_CAST "numberOfRows", BAD_CAST str);
  sprintf(str, "%d", info->quicklooks.numberOfColumns);
  xmlNewChild(node2, NULL, BAD_CAST "numberOfColumns", BAD_CAST str);
  double2str(info->quicklooks.columnBlockLength, 0, str);
  unit = xmlNewChild(node2, NULL, BAD_CAST "columnBlockLength", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "pixels");
  double2str(info->quicklooks.rowBlockLength, 0, str);
  unit = xmlNewChild(node2, NULL, BAD_CAST "rowBlockLength", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "pixels");
  double2str(info->quicklooks.rowSpacing, 0, str);
  unit = xmlNewChild(node2, NULL, BAD_CAST "rowSpacing", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(info->quicklooks.columnSpacing, 0, str);
  unit = xmlNewChild(node2, NULL, BAD_CAST "columnSpacing", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  /*
    node = xmlNewChild(parent, NULL, BAD_CAST "compositeQuicklook", NULL);
    xmlNewChild(node, NULL, BAD_CAST "imageDataFormat", 
    BAD_CAST info->compositeQLImageDataFormat);
    xmlNewChild(node, NULL, BAD_CAST "imageDataDepth", 
    iStr(info->compositeQLImageDataDepth));
    node2 = xmlNewChild(node, NULL, BAD_CAST "polLayerCode", NULL);
    
    info->compositeQLPolLayerCode = (iso_polColor *) MALLOC(sizeof(iso_polColor));
    strcpy(info->compositeQLPolLayerCode[0].polarization, "HH");
    strcpy(info->compositeQLPolLayerCode[0].color, "grey");
    
    for (ii=0; ii<info->numberOfLayers; ii++) {
    xmlNewChild(node2, NULL, BAD_CAST "polarization", 
    BAD_CAST info->compositeQLPolLayerCode[ii].polarization);
    xmlNewChild(node2, NULL, BAD_CAST "color",
    BAD_CAST info->compositeQLPolLayerCode[ii].color);
    }
  */
  node = xmlNewChild(parent, NULL, BAD_CAST "browseImage", NULL);
  xmlNewChild(node, NULL, BAD_CAST "imageDataFormat", 
	      BAD_CAST info->browseImageDataFormat);
  sprintf(str, "%d", info->browseImageDataDepth);
  xmlNewChild(node, NULL, BAD_CAST "imageDataDepth", BAD_CAST str);
  xmlNewChild(parent, NULL, BAD_CAST "mapPlotFormat", 
	      BAD_CAST info->mapPlotFormat);
  
  // Product Specific
  iso_productSpecific *spec = iso->productSpecific;
  section = xmlNewChild(root, NULL, BAD_CAST "productSpecific", NULL);
  parent = xmlNewChild(section, NULL, BAD_CAST "complexImageInfo", NULL);
  double2str(spec->commonPRF, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "commonPRF", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(spec->commonRSF, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "commonRSF", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(spec->slantRangeResolution, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "slantRangeResolution", 
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(spec->projectedSpacingAzimuth, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "projectedSpacingAzimuth",
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  node = xmlNewChild(parent, NULL, BAD_CAST "projectedSpacingRange", NULL);
  double2str(spec->projectedSpacingGroundNearRange, 3, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "groundNear", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(spec->projectedSpacingGroundFarRange, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "groundFar", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
  double2str(spec->projectedSpacingSlantRange, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "slantRange", BAD_CAST str);
  if (spec->imageCoordinateType == RAW_COORD)
    strcpy(str, "RAW");
  else if (spec->imageCoordinateType == ZERODOPPLER)
    strcpy(str, "ZERODOPPLER");
  else if (spec->imageCoordinateType == UNDEF_COORD)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "imageCoordinateType", BAD_CAST str);
  if (spec->imageDataStartWith == EARLYAZNEARRG)
    strcpy(str, "EARLYAZNEARRG");
  else if (spec->imageDataStartWith == EARLYAZFARRG)
    strcpy(str, "EARLYAZFARRG");
  else if (spec->imageDataStartWith == LATEAZNEARRG)
    strcpy(str, "LATEAZNEARRG");
  else if (spec->imageDataStartWith == LATEAZFARRG)
    strcpy(str, "LATEAZFARRG");
  else if (spec->imageDataStartWith == UNDEF_DATA_START)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "imageDataStartWith", BAD_CAST str);
  if (spec->quicklookDataStartWith == EARLYAZNEARRG)
    strcpy(str, "EARLYAZNEARRG");
  else if (spec->quicklookDataStartWith == EARLYAZFARRG)
    strcpy(str, "EARLYAZFARRG");
  else if (spec->quicklookDataStartWith == LATEAZNEARRG)
    strcpy(str, "LATEAZNEARRG");
  else if (spec->quicklookDataStartWith == LATEAZFARRG)
    strcpy(str, "LATEAZFARRG");
  else if (spec->quicklookDataStartWith == UNDEF_DATA_START)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "quicklookDataStartWith", BAD_CAST str);
  if (spec->geocodedImageInfoFlag) {
    parent = xmlNewChild(section, NULL, BAD_CAST "geocodedImageInfo", NULL);
    node = xmlNewChild(parent, NULL, BAD_CAST "mapProjection", NULL);
    xmlNewChild(node, NULL, BAD_CAST "geodeticDatumID", 
		BAD_CAST spec->geodeticDatumID);
    xmlNewChild(node, NULL, BAD_CAST "projectionID", 
		BAD_CAST spec->projectionID);
    xmlNewChild(node, NULL, BAD_CAST "zoneID", BAD_CAST spec->zoneID);
    node2 = xmlNewChild(node, NULL, BAD_CAST "projectionCenter", NULL);
    double2str(spec->projectionCenterLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "latitude", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->projectionCenterLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "longitude", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    node2 = xmlNewChild(node, NULL, BAD_CAST "mapOrigin", NULL);
    double2str(spec->mapOriginEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "easting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->mapOriginNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "northing", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->scaleFactor, 0, str);
    xmlNewChild(node, NULL, BAD_CAST "scaleFactor", BAD_CAST str);
    node  = xmlNewChild(parent, NULL, BAD_CAST "geoParameter", NULL);
    node2 = xmlNewChild(node, NULL, BAD_CAST "pixelSpacing", NULL);
    double2str(spec->pixelSpacingEasting, 0, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "easting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->pixelSpacingNorthing, 0, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "northing", BAD_CAST str);
    node2 = xmlNewChild(node, NULL, BAD_CAST "frameCoordsGeographic", NULL);
    double2str(spec->frameCoordsGeographic.upperLeftLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftLatitude", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.upperLeftLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftLongitude", 
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.upperRightLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightLatitude", 
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.upperRightLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.lowerLeftLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftLatitude", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.lowerLeftLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.lowerRightLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightLatitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->frameCoordsGeographic.lowerRightLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    node2 = xmlNewChild(node, NULL, BAD_CAST "frameCoordsCartographic", NULL);
    double2str(spec->frameCoordsCartographic.upperLeftEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.upperLeftNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftNorthing", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.upperRightEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.upperRightNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightNorthing",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.lowerRightEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightEasting",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.lowerRightNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightNorthing",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.lowerLeftEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->frameCoordsCartographic.lowerLeftNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftNorthing", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    node2 = xmlNewChild(node, NULL, BAD_CAST "sceneCoordsGeographic", NULL);
    double2str(spec->sceneCoordsGeographic.upperLeftLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftLatitude", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.upperLeftLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.upperRightLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightLatitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.upperRightLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.lowerLeftLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftLatitude", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.lowerLeftLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.lowerRightLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightLatitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCoordsGeographic.lowerRightLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightLongitude", 
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    node2 = xmlNewChild(node, NULL, BAD_CAST "sceneCoordsCartographic", NULL);
    double2str(spec->sceneCoordsCartographic.upperLeftEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.upperLeftNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperLeftNorthing", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.upperRightEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.upperRightNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "upperRightNorthing", 
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.lowerRightEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.lowerRightNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerRightNorthing", 
		       BAD_CAST str); 
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.lowerLeftEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftEasting", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCoordsCartographic.lowerLeftNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lowerLeftNorthing", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    node2 = xmlNewChild(node, NULL, BAD_CAST "sceneCenterCoordsGeographic", 
			NULL);
    double2str(spec->sceneCenterCoordLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "centerCoordLatitude", 
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(spec->sceneCenterCoordLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "centerCoordLongitude",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    node2 = xmlNewChild(node, NULL, BAD_CAST "sceneCenterCoordsCartograhic", 
			NULL);
    double2str(spec->sceneCenterCoordEasting, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "centerCoordEasting", 
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(spec->sceneCenterCoordNorthing, 3, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "centerCoordNorthing",
		       BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    if (spec->imageResamplingMethod == NEAREST_NEIGHBOR_RESAMPLE)
      strcpy(str, "NEAREST NEIGHBOR");
    else if (spec->imageResamplingMethod == BILINEAR_RESAMPLE)
      strcpy(str, "BILINEAR");
    else if (spec->imageResamplingMethod == CUBIC_CONVOLUTION_RESAMPLE)
      strcpy(str, "CUBIC CONVOLUTION");
    else if (spec->imageResamplingMethod == UNDEF_RESAMPLE)
      strcpy(str, "UNDEFINED");
    xmlNewChild(node, NULL, BAD_CAST "imageResamplingMethod", BAD_CAST str);
    if (spec->elevationDataFlag) {
      node = xmlNewChild(parent, NULL, BAD_CAST "elevationData", NULL);
      xmlNewChild(node, NULL, BAD_CAST "dataSource", 
		  BAD_CAST spec->elevationDataSource);
      double2str(spec->elevationMinimumHeight, 3, str);
      unit = xmlNewChild(node, NULL, BAD_CAST "minimumHeight", BAD_CAST str);
      xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
      double2str(spec->elevationMeanHeight, 3, str);
      unit = xmlNewChild(node, NULL, BAD_CAST "meanHeight", BAD_CAST str);
      xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
      double2str(spec->elevationMaximumHeight, 3, str);
      unit = xmlNewChild(node, NULL, BAD_CAST "maximumHeight", BAD_CAST str);
      xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    }
    if (spec->incidenceAngleMaskDescriptionFlag) {
      node = xmlNewChild(parent, NULL, BAD_CAST "incidenceAngleMaskDescription",
			 NULL);
      xmlNewChild(node, NULL, BAD_CAST "pixelValueID", 
		  BAD_CAST spec->incidenceAnglePixelValueID);
      if (spec->incidenceAngleImageDataFormat == CEOS_DATA_FORMAT)
	strcpy(str, "CEOS");
      else if (spec->incidenceAngleImageDataFormat == GEOTIFF_DATA_FORMAT)
	strcpy(str, "GEOTIFF");
      else if (spec->incidenceAngleImageDataFormat == HDF5_DATA_FORMAT)
	strcpy(str, "HDF5");
      else if (spec->incidenceAngleImageDataFormat == COSAR_DATA_FORMAT)
	strcpy(str, "COSAR");
      else if (spec->incidenceAngleImageDataFormat == UNDEF_DATA_FORMAT)
	strcpy(str, "UNDEFINED");
      xmlNewChild(node, NULL, BAD_CAST "imageDataFormat", BAD_CAST str);
      sprintf(str, "%d", spec->incidenceAngleImageDataDepth);
      xmlNewChild(node, NULL, BAD_CAST "imageDataDepth", BAD_CAST str);
      node2 = xmlNewChild(node, NULL, BAD_CAST "imageRaster", NULL);
      sprintf(str, "%d", spec->incidenceAngleNumberOfRows);
      xmlNewChild(node2, NULL, BAD_CAST "numberOfRows", BAD_CAST str);
      sprintf(str, "%d", spec->incidenceAngleNumberOfColumns);
      xmlNewChild(node2, NULL, BAD_CAST "numberOfColumns", BAD_CAST str);
      double2str(spec->incidenceAngleRowSpacing, 0, str);
      unit = xmlNewChild(node2, NULL, BAD_CAST "rowSpacing", BAD_CAST str);
      xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
      double2str(spec->incidenceAngleColumnSpacing, 0, str);
      unit = xmlNewChild(node2, NULL, BAD_CAST "columnSpacing", BAD_CAST str);
      xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    }
  }
  
  // Setup
  iso_setup *setup = iso->setup;
  section = xmlNewChild(root, NULL, BAD_CAST "setup", NULL);
  parent = xmlNewChild(section, NULL, BAD_CAST "orderInfo", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "orderType", BAD_CAST setup->orderType);
  xmlNewChild(parent, NULL, BAD_CAST "processingPriority", 
	      BAD_CAST setup->processingPriority);
  if (setup->orbitAccuracy == PREDICTED_ORBIT)
    strcpy(str, "PREDICTED");
  else if (setup->orbitAccuracy == RESTITUTED_ORBIT)
    strcpy(str, "RESTITUTED");
  else if (setup->orbitAccuracy == PRECISE_ORBIT)
    strcpy(str, "PRECISE");
  else if (setup->orbitAccuracy == TLE)
    strcpy(str, "TLE");
  else if (setup->orbitAccuracy == UNDEF_ORBIT_ACC)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "orbitAccuracy", BAD_CAST str);
  if (setup->sceneSpecification == FRAME_SPEC)
    strcpy(str, "FRAME");
  else if (setup->sceneSpecification == TIME_SPEC)
    strcpy(str, "TIME");
  else if (setup->sceneSpecification == CENTERCOORDS_SPEC)
    strcpy(str, "CENTERCOORDS");
  else if (setup->sceneSpecification == UNDEF_SCENE_SPEC)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "sceneSpecification", BAD_CAST str);
  node = xmlNewChild(parent, NULL, BAD_CAST "orderedScene", NULL);
  if (setup->sceneSpecification == FRAME_SPEC) {
    sprintf(str, "%d", setup->frameID);
    xmlNewChild(node, NULL, BAD_CAST "frameID", BAD_CAST str);
  }
  else if (setup->sceneSpecification == TIME_SPEC) {
    node2 = xmlNewChild(node, NULL, BAD_CAST "sceneExtent", NULL);
    dateTime2str(setup->sceneStartTimeUTC, str);
    xmlNewChild(node2, NULL, BAD_CAST "startTimeUTC", BAD_CAST str);
    dateTime2str(setup->sceneStopTimeUTC, str);
    xmlNewChild(node2, NULL, BAD_CAST "stopTimeUTC", BAD_CAST str);
  }
  else if (setup->sceneSpecification == CENTERCOORDS_SPEC) {
    node2 = xmlNewChild(node, NULL, BAD_CAST "sceneCenterCoord", NULL);
    double2str(setup->sceneCenterLatitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lat", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
    double2str(setup->sceneCenterLongitude, 5, str);
    unit = xmlNewChild(node2, NULL, BAD_CAST "lon", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "degrees");
  }
  if (setup->imagingMode == FINE_BEAM)
    strcpy(str, "FINE BEAM");
  else if (setup->imagingMode == STANDARD_BEAM)
    strcpy(str, "STANDARD BEAM");
  else if (setup->imagingMode == STRIPMAP_IMAGE)
    strcpy(str, "STRIPMAP");
  else if (setup->imagingMode == SCANSAR_IMAGE)
    strcpy(str, "SCANSAR");
  else if (setup->imagingMode == SPOTLIGHT_IMAGE)
    strcpy(str, "SPOTLIGHT");
  else if (setup->imagingMode == UNDEF_IMAGE_MODE)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "imagingMode", BAD_CAST str);
  if (setup->lookDirection == RIGHT_LOOK)
    strcpy(str, "RIGHT");
  else if (setup->lookDirection == LEFT_LOOK)
    strcpy(str, "LEFT");
  else if (setup->lookDirection == UNDEF_LOOK)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "lookDirection", BAD_CAST str);
  if (setup->polarizationMode == SINGLE_POL)
    strcpy(str, "SINGLE");
  else if (setup->polarizationMode == DUAL_POL)
    strcpy(str, "DUAL");
  else if (setup->polarizationMode == QUAD_POL)
    strcpy(str, "QUAD");
  else if (setup->polarizationMode == UNDEF_POL_MODE)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "polarizationMode", BAD_CAST str);
  if (setup->polLayer == HH_POL)
    strcpy(str, "HH");
  else if (setup->polLayer == HV_POL)
    strcpy(str, "HV");
  else if (setup->polLayer == VH_POL)
    strcpy(str, "VH");
  else if (setup->polLayer == VV_POL)
    strcpy(str, "VV");
  else if (setup->polLayer == UNDEF_POL_LAYER)
    strcpy(str, "UNDEFINED");
  node2 = xmlNewChild(node, NULL, BAD_CAST "polList", NULL);
  xmlNewChild(node2, NULL, BAD_CAST "polLayer", BAD_CAST str);
  xmlNewChild(node, NULL, BAD_CAST "elevationBeamConfiguration", 
	      BAD_CAST setup->elevationBeamConfiguration);
  if (setup->productVariant == SLC_PRODUCT)
    strcpy(str, "SLC");
  else if (setup->productVariant == STD_PRODUCT)
    strcpy(str, "STD");
  else if (setup->productVariant == TC_PRODUCT)
    strcpy(str, "TC");
  else if (setup->productVariant == RTC_PRODUCT)
    strcpy(str, "RTC");
  else if (setup->productVariant == GEO_PRODUCT)
    strcpy(str, "GEO");
  else if (setup->productVariant == SSC_PRODUCT)
    strcpy(str, "SSC");
  else if (setup->productVariant == MGD_PRODUCT)
    strcpy(str, "MGD");
  else if (setup->productVariant == GEC_PRODUCT)
    strcpy(str, "GEC");
  else if (setup->productVariant == EEC_PRODUCT)
    strcpy(str, "EEC");
  else if (setup->productVariant == UNDEF_PRODUCT)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "productVariant", BAD_CAST str);
  if (setup->resolutionVariant == SE_RES)
    strcpy(str, "SE");
  else if (setup->resolutionVariant == RE_RES)
    strcpy(str, "RE");
  else if (setup->resolutionVariant == UNDEF_RES)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "resolutionVariant", BAD_CAST str);
  if (setup->projection == SLANTRANGE_PROJ)
    strcpy(str, "SLANTRANGE");
  else if (setup->projection == GROUNDRANGE_PROJ)
    strcpy(str, "GROUNDRANGE");
  else if (setup->projection == MAP_PROJ)
    strcpy(str, "MAP");
  else if (setup->projection == UNDEF_PROJ)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "projection", BAD_CAST str);
  parent = xmlNewChild(section, NULL, BAD_CAST "inputData", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "logicalDataTakeID", 
	      BAD_CAST setup->logicalDataTakeID);
  xmlNewChild(parent, NULL, BAD_CAST "level0ProductID", 
	      BAD_CAST setup->level0ProductID);
  dateTime2str(setup->L0SARGenerationTimeUTC, str);
  xmlNewChild(parent, NULL, BAD_CAST "L0SARGenerationTimeUTC", BAD_CAST str);
  if (setup->numProcessingSteps > 0) {
    parent = xmlNewChild(section, NULL, BAD_CAST "processingSteps", NULL);
    char step[5];
    for (ii=0; ii<setup->numProcessingSteps; ii++) {
      sprintf(step, "%ld", ii+1);
      node = xmlNewChild(parent, NULL, BAD_CAST "software", NULL);
      xmlNewProp(node, BAD_CAST "step", BAD_CAST step);
      xmlNewChild(node, NULL, BAD_CAST "softwareID", 
		  BAD_CAST setup->processingStep[ii].softwareID);
      xmlNewChild(node, NULL, BAD_CAST "softwareVersion",
		  BAD_CAST setup->processingStep[ii].softwareVersion);
      dateTime2str(setup->processingStep[ii].processingTimeUTC, str);
      xmlNewChild(node, NULL, BAD_CAST "processingTimeUTC", BAD_CAST str);  
      if (setup->processingStep[ii].algorithm)
	xmlNewChild(node, NULL, BAD_CAST "algorithm", 
		    BAD_CAST setup->processingStep[ii].algorithm);
    }
  }
  
  // Processing
  iso_processing *proc = iso->processing;
  section = xmlNewChild(root, NULL, BAD_CAST "processing", NULL);
  parent = xmlNewChild(section, NULL, BAD_CAST "doppler", NULL);
  xmlNewChild(parent, NULL, BAD_CAST "dopplerBasebandEstimationMethod",
	      BAD_CAST proc->dopplerBasebandEstimationMethod);
  if (proc->dopplerCentroidCoordinateType == RAW_COORD)
    strcpy(str, "RAW");
  else if (proc->dopplerCentroidCoordinateType == ZERODOPPLER)
    strcpy(str, "ZERODOPPLER");
  else if (proc->dopplerCentroidCoordinateType == UNDEF_COORD)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "dopplerCentroidCoordinateType", 
	      BAD_CAST str);
  if (proc->doppler) {
    for (ii=0; ii<comps->numLayers; ii++) {
      node = xmlNewChild(parent, NULL, BAD_CAST "dopplerCentroid", NULL);
      sprintf(str, "%ld", ii+1);
      xmlNewProp(node, BAD_CAST "layerIndex", BAD_CAST str);
      if (proc->doppler[ii].polLayer == HH_POL)
	strcpy(str, "HH");
      else if (proc->doppler[ii].polLayer == HV_POL)
	strcpy(str, "HV");
      else if (proc->doppler[ii].polLayer == VH_POL)
	strcpy(str, "VH");
      else if (proc->doppler[ii].polLayer == VV_POL)
	strcpy(str, "VV");
      else if (proc->doppler[ii].polLayer == UNDEF_POL_LAYER)
	strcpy(str, "UNDEFINED");
      xmlNewChild(node, NULL, BAD_CAST "polLayer", BAD_CAST str);
      sprintf(str, "%d", proc->doppler[ii].numberOfBlocks);
      xmlNewChild(node, NULL, BAD_CAST "numberOfBlocks", BAD_CAST str);
      sprintf(str, "%d", proc->doppler[ii].numberOfRejectedBlocks);
      xmlNewChild(node, NULL, BAD_CAST "numberOfRejectedBlocks", BAD_CAST str);
      sprintf(str, "%d", proc->doppler[ii].numberOfDopperRecords);
      xmlNewChild(node, NULL, BAD_CAST "numberOfDopplerRecords", BAD_CAST str);
      node2 = xmlNewChild(node, NULL, BAD_CAST "dopplerEstimate", NULL);
      dateTime2str(proc->doppler[ii].timeUTC, str);
      xmlNewChild(node2, NULL, BAD_CAST "timeUTC", BAD_CAST str);
      double2str(proc->doppler[ii].dopplerAtMidRange, 0, str);
      xmlNewChild(node2, NULL, BAD_CAST "dopplerAtMidRange", BAD_CAST str);
      int degree = proc->doppler[ii].polynomialDegree;
      sprintf(str, "%d", degree);
      xmlNewChild(node2, NULL, BAD_CAST "polynomialDegree", BAD_CAST str);
      node3 = xmlNewChild(node2, NULL, BAD_CAST "basebandDoppler", NULL);
      for (kk=0; kk<=degree; kk++) {
	double2str(proc->doppler[ii].coefficient[kk], 0, str);
	unit = xmlNewChild(node3, NULL, BAD_CAST "coefficient", BAD_CAST str);
	sprintf(str, "%ld", kk);
	xmlNewProp(unit, BAD_CAST "exponent", BAD_CAST str);
      }
    }
  }
  parent = xmlNewChild(section, NULL, BAD_CAST "processingParameter", NULL);
  if (proc->processingParameter[0].processingInfoCoordinateType == RAW_COORD)
    strcpy(str, "RAW");
  else if (proc->processingParameter[0].processingInfoCoordinateType == ZERODOPPLER)
    strcpy(str, "ZERODOPPLER");
  else if (proc->processingParameter[0].processingInfoCoordinateType == UNDEF_COORD)
    strcpy(str, "UNDEFINED");
  xmlNewChild(parent, NULL, BAD_CAST "processingInfoCoordinateType", 
	      BAD_CAST str);
  double2str(proc->processingParameter[0].rangeLooks, 0, str);
  xmlNewChild(parent, NULL, BAD_CAST "rangeLooks", BAD_CAST str);
  double2str(proc->processingParameter[0].azimuthLooks, 0, str);
  xmlNewChild(parent, NULL, BAD_CAST "azimuthLooks", BAD_CAST str);
  double2str(proc->processingParameter[0].rangeLookBandwidth, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "rangeLookBandwidth", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(proc->processingParameter[0].azimuthLookBandwidth, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "azimuthLookBandwidth", 
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(proc->processingParameter[0].totalProcessedRangeBandwidth, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "totalProcessedRangeBandwidth",
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(proc->processingParameter[0].totalProcessedAzimuthBandwidth, 0, 
	     str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "totalProcessedAzimuthBandwidth",
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(proc->processingParameter[0].chirpRate, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "chirpRate", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz/sec");
  double2str(proc->processingParameter[0].pulseDuration, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "pulseDuration", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");


  parent = xmlNewChild(section, NULL, BAD_CAST "processingFlags", NULL);
  if (proc->chirpReplicaUsedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "chirpReplicaUsedFlag", BAD_CAST str);
  if (proc->geometricDopplerUsedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "geometricDopplerUsedFlag", BAD_CAST str);
  if (proc->azimuthPatternCorrectedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "azimuthPatternCorrectedFlag", 
	      BAD_CAST str);
  if (proc->elevationPatternCorrectedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "elevationPatternCorrectedFlag",
	      BAD_CAST str);
  if (proc->detectedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "detectedFlag", BAD_CAST str);
  if (proc->multiLookedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "multiLookedFlag", BAD_CAST str);
  if (proc->polarimetricProcessedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "polarimetricProcessedFlag", BAD_CAST str);
  if (proc->terrainCorrectedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "terrainCorrectedFlag", BAD_CAST str);
  if (proc->layoverShadowMaskGeneratedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "layoverShadowMaskGeneratedFlag",
	      BAD_CAST str);
  if (proc->geocodedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "geocodedFlag", BAD_CAST str);
  if (proc->nominalProcessingPerformedFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "nominalProcessingPerformedFlag", 
	      BAD_CAST str);

  // Instrument
  iso_instrument *inst = iso->instrument;
  section = xmlNewChild(root, NULL, BAD_CAST "instrument", NULL);
  if (inst->instrumentInfoCoordinateType == RAW_COORD)
    strcpy(str, "RAW");
  else if (inst->instrumentInfoCoordinateType == ZERODOPPLER)
    strcpy(str, "ZERODOPPLER");
  else if (inst->instrumentInfoCoordinateType == UNDEF_COORD)
    strcpy(str, "UNDEFINED");
  xmlNewChild(section, NULL, BAD_CAST "instrumentInfoCoordinateType", 
	      BAD_CAST str);
  parent = xmlNewChild(section, NULL, BAD_CAST "radarParameters", NULL);
  double2str(inst->centerFrequency, 0, str);
  unit = xmlNewChild(parent, NULL, BAD_CAST "centerFrequency", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  parent = xmlNewChild(section, NULL, BAD_CAST "settings", NULL);
  if (inst->settings) {
    for (ii=0; ii<comps->numLayers; ii++) {
      if (inst->settings[ii].polLayer == HH_POL)
	strcpy(str, "HH");
      else if (inst->settings[ii].polLayer == HV_POL)
	strcpy(str, "HV");
      else if (inst->settings[ii].polLayer == VH_POL)
	strcpy(str, "VH");
      else if (inst->settings[ii].polLayer == VV_POL)
	strcpy(str, "VV");
      else if (inst->settings[ii].polLayer == UNDEF_POL_LAYER)
	strcpy(str, "UNDEFINED");
      xmlNewChild(parent, NULL, BAD_CAST "polLayer", BAD_CAST str);
      xmlNewChild(parent, NULL, BAD_CAST "beamID", 
		  BAD_CAST inst->settings[ii].beamID);
      double2str(inst->settings[ii].rxBandwidth, 0, str);
      unit = xmlNewChild(parent, NULL, BAD_CAST "rxBandwidth", BAD_CAST str);
      xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
      double2str(inst->settings[ii].rsf, 0, str);
      unit = xmlNewChild(parent, NULL, BAD_CAST "RSF", BAD_CAST str);
      
      inst->settings[ii].numberOfPRFChanges = 0;
      inst->settings[ii].numberOfEchoWindowPositionChanges = 0;
      inst->settings[ii].numberOfEchoWindowLengthChanges = 0;
      inst->settings[ii].numberOfSettingRecords = 1;
      
      sprintf(str, "%d", inst->settings[ii].numberOfPRFChanges);
      xmlNewChild(parent, NULL, BAD_CAST "numberOfPRFChanges", BAD_CAST str);
      sprintf(str, "%d", inst->settings[ii].numberOfEchoWindowPositionChanges);
      xmlNewChild(parent, NULL, BAD_CAST "numberOfEchoWindowPositionChanges",
		  BAD_CAST str);
      sprintf(str, "%d", inst->settings[ii].numberOfEchoWindowLengthChanges);
      xmlNewChild(parent, NULL, BAD_CAST "numberOfEchoWindowLengthChanges",
		  BAD_CAST str);
      sprintf(str, "%d", inst->settings[ii].numberOfSettingRecords);
      xmlNewChild(parent, NULL, BAD_CAST "numberOfSettingRecords", 
		  BAD_CAST str);
      node = xmlNewChild(parent, NULL, BAD_CAST "settingRecords", NULL);
      iso_settingRecord *rec = inst->settings[ii].settingRecord;
      for (kk=0; kk<inst->settings[ii].numberOfSettingRecords; kk++) {
	node2 = xmlNewChild(node, NULL, BAD_CAST "dataSegment", NULL);
	sprintf(str, "%ld", kk+1);
	xmlNewProp(node2, BAD_CAST "segmentID", BAD_CAST str);
	dateTime2str(rec[kk].startTimeUTC, str);
	xmlNewChild(node2, NULL, BAD_CAST "startTimeUTC", BAD_CAST str);
	dateTime2str(rec[kk].stopTimeUTC, str);
	xmlNewChild(node2, NULL, BAD_CAST "stopTimeUTC", BAD_CAST str);
	sprintf(str, "%d", rec[kk].numberOfRows);
	xmlNewChild(node2, NULL, BAD_CAST "numberOfRows", BAD_CAST str);
	double2str(rec[kk].prf,0, str);
	unit = xmlNewChild(node, NULL, BAD_CAST "PRF", BAD_CAST str);
	xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
	double2str(rec[kk].echoWindowPosition, 0, str); 
	xmlNewChild(node, NULL, BAD_CAST "echoWindowPosition", BAD_CAST str);
	double2str(rec[kk].echoWindowLength, 0, str); 
	xmlNewChild(node, NULL, BAD_CAST "echoWindowLength", BAD_CAST str);
	xmlNewChild(node, NULL, BAD_CAST "pulseType", 
		    BAD_CAST rec[kk].pulseType);
      }
    }
  }
  
  // Calibration
  // section = xmlNewChild(root, NULL, "calibration", NULL);
  
  // Platform
  iso_platform *platform = iso->platform;
  section = xmlNewChild(root, NULL, BAD_CAST "platform", NULL);
  parent = xmlNewChild(section, NULL, BAD_CAST "orbit", NULL);
  node = xmlNewChild(parent, NULL, BAD_CAST "orbitHeader", NULL);
  if (platform->sensor == PREDICTED_SENSOR)
    strcpy(str, "PREDICTED SENSOR");
  else if (platform->sensor == SINGLE_GPS)
    strcpy(str, "SINGLE GPS");
  else if (platform->sensor == DIFFERENTIAL_GPS)
    strcpy(str, "DIFFERENTIAL GPS");
  else if (platform->sensor == UNDEF_ORBIT_SENSOR)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "sensor", BAD_CAST str);
  if (platform->accuracy == PREDICTED_ORBIT)
    strcpy(str, "PREDICTED");
  else if (platform->accuracy == RESTITUTED_ORBIT)
    strcpy(str, "RESTITUTED");
  else if (platform->accuracy == PRECISE_ORBIT)
    strcpy(str, "PRECISE");
  else if (platform->accuracy == TLE)
    strcpy(str, "TLE");
  else if (platform->accuracy == UNDEF_ORBIT_ACC)
    strcpy(str, "UNDEFINED");
  xmlNewChild(node, NULL, BAD_CAST "accuracy", BAD_CAST str);

  sprintf(str, "%ld", platform->numStateVectors);
  xmlNewChild(node, NULL, BAD_CAST "numStateVectors", BAD_CAST str);
  dateTime2str(platform->firstStateTimeUTC, str);
  xmlNewChild(node, NULL, BAD_CAST "firstStateTimeUTC", BAD_CAST str);
  dateTime2str(platform->lastStateTimeUTC, str);
  xmlNewChild(node, NULL, BAD_CAST "lastStateTimeUTC", BAD_CAST str);
  xmlNewChild(node, NULL, BAD_CAST "stateVectorRefFrame",
	      BAD_CAST platform->stateVectorRefFrame);
  double2str(platform->stateVectorTimeSpacing, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "stateVectorTimeSpacing", 
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "s");
  char num[5];
  for (ii=0; ii<platform->numStateVectors; ii++) {
    node = xmlNewChild(parent, NULL, BAD_CAST "stateVec", NULL);
    sprintf(num, "%ld", ii+1); 
    xmlNewProp(node, BAD_CAST "num", BAD_CAST num);
    dateTime2str(platform->stateVec[ii].timeUTC, str);
    xmlNewChild(node, NULL, BAD_CAST "timeUTC", BAD_CAST str);
    double2str(platform->stateVec[ii].posX, 3, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "posX", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(platform->stateVec[ii].posY, 3, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "posY", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(platform->stateVec[ii].posZ, 3, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "posZ", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m");
    double2str(platform->stateVec[ii].velX, 3, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "velX", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m/s");
    double2str(platform->stateVec[ii].velY, 3, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "velY", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m/s");
    double2str(platform->stateVec[ii].velZ, 3, str);
    unit = xmlNewChild(node, NULL, BAD_CAST "velZ", BAD_CAST str);
    xmlNewProp(unit, BAD_CAST "units", BAD_CAST "m/s");
  }

  // Product Quality
  iso_productQuality *quality = iso->productQuality;
  section = xmlNewChild(root, NULL, BAD_CAST "productQuality", NULL);
  int numGaps;
  parent = xmlNewChild(section, NULL, BAD_CAST "rawDataQuality", NULL);
  if (quality->rawDataQuality) {
    for (ii=0; ii<info->numberOfLayers; ii++) {
      if (quality->rawDataQuality[ii].polLayer == HH_POL)
	strcpy(str, "HH");
      else if (quality->rawDataQuality[ii].polLayer == HV_POL)
	strcpy(str, "HV");
      else if (quality->rawDataQuality[ii].polLayer == VH_POL)
	strcpy(str, "VH");
      else if (quality->rawDataQuality[ii].polLayer == VV_POL)
	strcpy(str, "VV");
      else if (quality->rawDataQuality[ii].polLayer == UNDEF_POL_LAYER)
	strcpy(str, "UNDEFINED");    
      xmlNewChild(parent, NULL, BAD_CAST "polLayer", BAD_CAST str);
      if (quality->rawDataQuality[ii].beamID)
	xmlNewChild(parent, NULL, BAD_CAST "beamID", 
		    BAD_CAST quality->rawDataQuality[ii].beamID);
      numGaps = quality->rawDataQuality[ii].numGaps;
      sprintf(str, "%d", numGaps);
      xmlNewChild(parent, NULL, BAD_CAST "numGaps", BAD_CAST str);
      for (kk=0; kk<numGaps; kk++) {
	node = xmlNewChild(parent, NULL, BAD_CAST "gap", NULL);
	sprintf(num, "%ld", kk+1);
	xmlNewProp(node, BAD_CAST "num", BAD_CAST num);
	sprintf(str, "%ld", quality->rawDataQuality[ii].gap[kk].start);
	xmlNewChild(node, NULL, BAD_CAST "start", BAD_CAST str);
	sprintf(str, "%d", quality->rawDataQuality[ii].gap[kk].length);
	xmlNewChild(node, NULL, BAD_CAST "length", BAD_CAST str);
	if (quality->rawDataQuality[ii].gap[kk].fill == RANDOM_FILL)
	  strcpy(str, "RANDOM");
	else if (quality->rawDataQuality[ii].gap[kk].fill == ZERO_FILL)
	  strcpy(str, "ZERO");
	else if (quality->rawDataQuality[ii].gap[kk].fill == UNDEF_FILL)
	  strcpy(str, "UNDEFINED");
	xmlNewChild(node, NULL, BAD_CAST "fill", BAD_CAST str);
      }
      if (quality->rawDataQuality[ii].gapSignificanceFlag)
	strcpy(str, "true");
      else
	strcpy(str, "false");
      xmlNewChild(parent, NULL, BAD_CAST "gapSignificanceFlag", BAD_CAST str);
      if (quality->rawDataQuality[ii].missingLinesSignificanceFlag)
	strcpy(str, "true");
      else
	strcpy(str, "false");
      xmlNewChild(parent, NULL, BAD_CAST "missingLinesSignificanceFlag",
		  BAD_CAST str);
      if (quality->rawDataQuality[ii].bitErrorSignificanceFlag)
	strcpy(str, "true");
      else
	strcpy(str, "false");
      xmlNewChild(parent, NULL, BAD_CAST "bitErrorSignificanceFlag", 
		  BAD_CAST str);
      if (quality->rawDataQuality[ii].timeReconstructionSignificanceFlag)
	strcpy(str, "true");
      else
	strcpy(str, "false");
      xmlNewChild(parent, NULL, BAD_CAST "timeReconstructionSignificanceFlag",
		  BAD_CAST str);
    }
  }
  parent = xmlNewChild(section, NULL, BAD_CAST "processingParameterQuality", 
		       NULL);
  if (quality->dopplerAmbiguityNotZeroFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "dopplerAmbiguityNotZeroFlag", 
	      BAD_CAST str);
  if (quality->dopplerOutsideLimitsFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "dopplerOutsideLimitsFlag", BAD_CAST str);
  if (quality->geolocationQualityLowFlag)
    strcpy(str, "true");
  else
    strcpy(str, "false");
  xmlNewChild(parent, NULL, BAD_CAST "geolocationQualityLowFlag", BAD_CAST str);
  if (quality->imageDataQuality) {
    for (ii=0; ii<info->numberOfLayers; ii++) {
      parent = xmlNewChild(section, NULL, BAD_CAST "imageDataQuality", NULL);
      sprintf(num, "%ld", ii+1);
      xmlNewProp(parent, BAD_CAST "layerIndex", BAD_CAST num);
      if (quality->imageDataQuality[ii].polLayer == HH_POL)
	strcpy(str, "HH");
      else if (quality->imageDataQuality[ii].polLayer == HV_POL)
	strcpy(str, "HV");
      else if (quality->imageDataQuality[ii].polLayer == VH_POL)
	strcpy(str, "VH");
      else if (quality->imageDataQuality[ii].polLayer == VV_POL)
	strcpy(str, "VV");
      else if (quality->imageDataQuality[ii].polLayer == UNDEF_POL_LAYER)
	strcpy(str, "UNDEFINED");    
      xmlNewChild(parent, NULL, BAD_CAST "polLayer", BAD_CAST str);
      if (quality->imageDataQuality[ii].beamID)
	xmlNewChild(parent, NULL, BAD_CAST "beamID", 
		    BAD_CAST quality->imageDataQuality[ii].beamID);
      node = xmlNewChild(parent, NULL, BAD_CAST "imageDataStatistics", NULL);
      double2str(quality->imageDataQuality[ii].min, 0, str); 
      xmlNewChild(node, NULL, BAD_CAST "min", BAD_CAST str);
      double2str(quality->imageDataQuality[ii].max, 0, str); 
      xmlNewChild(node, NULL, BAD_CAST "max", BAD_CAST str);
      double2str(quality->imageDataQuality[ii].mean, 0, str); 
      xmlNewChild(node, NULL, BAD_CAST "mean", BAD_CAST str);
      double2str(quality->imageDataQuality[ii].stdDev, 0, str);
      xmlNewChild(node, NULL, BAD_CAST "standardDeviation", BAD_CAST str);
      sprintf(str, "%d", quality->imageDataQuality[ii].missingLines);
      xmlNewChild(node, NULL, BAD_CAST "missingLines", BAD_CAST str);
      double2str(quality->imageDataQuality[ii].bitErrorRate, 0, str);
      xmlNewChild(node, NULL, BAD_CAST "bitErrorRate", BAD_CAST str);
      double2str(quality->imageDataQuality[ii].noData, 0, str);
      xmlNewChild(node, NULL, BAD_CAST "noData", BAD_CAST str);
    }
  }
  parent = xmlNewChild(section, NULL, BAD_CAST "limits", NULL);
  node = xmlNewChild(parent, NULL, BAD_CAST "rawData", NULL);
  sprintf(str, "%d", quality->gapDefinition);
  xmlNewChild(node, NULL, BAD_CAST "gapDefinition", BAD_CAST str);
  double2str(quality->gapPercentageLimit, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "gapPercentageLimit", BAD_CAST str);
  double2str(quality->missingLinePercentageLimit, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "missingLinePercentageLimit", BAD_CAST str);
  double2str(quality->bitErrorLimit, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "bitErrorLimit", BAD_CAST str);
  double2str(quality->timeReconstructionPercentageLimit, 0, str);
  xmlNewChild(node, NULL, BAD_CAST "timeReconstructionPercentageLimit",
	      BAD_CAST str);
  node = xmlNewChild(parent, NULL, BAD_CAST "processing", NULL);
  double2str(quality->dopplerCentroidLimit, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "dopplerCentroidLimit", BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "Hz");
  double2str(quality->geolocationQualityLimit, 0, str);
  unit = xmlNewChild(node, NULL, BAD_CAST "geolocationQualityLimit", 
		     BAD_CAST str);
  xmlNewProp(unit, BAD_CAST "units", BAD_CAST "arcsec");

  quality->instrumentStateRemark = (char *) MALLOC(sizeof(char)*1024);
  strcpy(quality->instrumentStateRemark, MAGIC_UNSET_STRING);

  if (quality->instrumentStateRemark)
    xmlNewChild(section, NULL, BAD_CAST "instrumentStateRemark", 
		BAD_CAST quality->instrumentStateRemark);

  // Save tree to file
  xmlSaveFormatFileEnc(outFile, doc, "UTF-8", 1);

  // Clean up
  xmlFreeDoc(doc);
  xmlCleanupParser();
  FREE(str);
}
