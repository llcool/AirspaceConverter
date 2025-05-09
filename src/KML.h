//============================================================================
// AirspaceConverter
// Since       : 14/6/2016
// Author      : Alberto Realis-Luc <alberto.realisluc@gmail.com>
// Web         : https://www.alus.it/AirspaceConverter
// Repository  : https://github.com/alus-it/AirspaceConverter.git
// Copyright   : (C) 2016 Alberto Realis-Luc
// License     : GNU GPL v3
//
// This source file is part of AirspaceConverter project
//============================================================================

#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <boost/property_tree/ptree_fwd.hpp>

class Altitude;
class Airspace;
class Waypoint;
class Airfield;

class KML {
public:
	KML(std::multimap<int, Airspace>& airspacesMap, std::multimap<int, Waypoint*>& waypointsMap);
	bool Write(const std::string& filename);
	inline bool WereAllAGLaltitudesCovered() const { return allAGLaltitudesCovered; }
	inline void ProcessLineStrings(bool LineStringAsAirspaces = true) { processLineString = LineStringAsAirspaces; }
	bool ReadKMZ(const std::string& filename);
	bool ReadKML(const std::string& filename);

private:
	static std::string PrepareTagText(const std::string& text);
	void WriteHeader(const bool airspacePresent, const bool waypointsPresent);
	void OpenPlacemark(const Airspace& airspace);
	void OpenPlacemark(const Waypoint* waypoint);
	void TitlePlacemark(const Airspace& airspace, const int type);
	void OpenPolygon(const bool extrude, const bool absolute);
	void ClosePolygon();
	void WriteSideWalls(const Airspace& airspace);
	void WriteSideWalls(const Airspace& airspace, const std::vector<double>& altitudesAmsl);
	void WriteBaseOrTop(const Airspace& airspace, const Altitude& alt, const bool extrudeToGround = false);
	void WriteBaseOrTop(const Airspace& airspace, const std::vector<double>& altitudesAmsl, const bool extrudeToGround = false);

	bool ProcessFolder(const boost::property_tree::ptree& folder, const int upperCategory);
	bool ProcessPlacemark(const boost::property_tree::ptree& placemark);
	static bool ProcessPolygon(const boost::property_tree::ptree& polygon, Airspace& airspace, bool& isExtruded, Altitude& avgAltitude);
	static bool ProcessCoordinates(const boost::property_tree::ptree& coordinates, Airspace& airspace, double& avgAltitude);
	static const std::string DetectIconsPath();

	static const std::string colors[];
	static const std::string airfieldColors[];
	static const std::string waypointIcons[];
	static const std::string iconsPath;
	std::multimap<int, Airspace>& airspaces;
	std::multimap<int, Waypoint*>& waypoints;
	std::ofstream outputFile;
	bool allAGLaltitudesCovered;
	bool processLineString;
	int folderCategory;
};
