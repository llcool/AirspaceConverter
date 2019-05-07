//============================================================================
// AirspaceConverter
// Since       : 14/6/2016
// Author      : Alberto Realis-Luc <alberto.realisluc@gmail.com>
// Web         : http://www.alus.it/AirspaceConverter
// Copyright   : (C) 2016-2019 Alberto Realis-Luc
// License     : GNU GPL v3
//
// This source file is part of AirspaceConverter project
//============================================================================

#include "SeeYou.h"
#include "AirspaceConverter.h"
#include "Waypoint.h"
#include "Airfield.h"
#include "Airspace.h"
#include "Geometry.h"
#include <fstream>
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/format.hpp>
#include <cmath>
#include <cassert>

SeeYou::SeeYou(std::multimap<int,Waypoint*>& waypointsMap):
	waypoints(waypointsMap) {
}

bool SeeYou::ParseLatitude(const std::string& text, double& lat) {
	const int len = (int)text.length();
	if(len < 5) return false;
	try {
		lat = std::stoi(text.substr(0,2));
		lat += std::stod(text.substr(2,len-3))/60;
	} catch(...) {
		return false;
	}
	const char sign = text.back();
	if (sign == 'S' || sign == 's') lat = -lat;
	else if (sign != 'N' && sign != 'n') return false;
	return true;
}

bool SeeYou::ParseLongitude(const std::string& text, double& lon) {
	const int len = (int)text.length();
	if(len < 6) return false;
	try {
		lon = std::stoi(text.substr(0,3));
		lon += std::stod(text.substr(3,len-4))/60;
	} catch (...) {
		return false;
	}
	const char sign = text.back();
	if (sign == 'W' || sign == 'w') lon = -lon;
	else if (sign != 'E' && sign != 'e') return false;
	return true;
}

bool SeeYou::ParseAltitude(const std::string& text, float& alt) {
	alt = 0;
	int pos = (int)text.length() - 1;
	if(pos == 0 && text.front()=='0') return true;
	if(pos<1) return false;
	bool feet = false;
	if(text.back() == 't' || text.back() == 'T') pos--;
	switch(text.at(pos)) {
		case 'm':
		case 'M':
			break;
		case 'f':
		case 'F':
			feet = true;
			break;
		default:
			return false;
	}
	try {
		alt = std::stof(text.substr(0,pos));
		if(feet) alt *= (float)Altitude::FEET2METER;
		return true;
	} catch(...) {}
	return false;
}

bool SeeYou::ParseStyle(const std::string& text, int& type) {
	if (!text.empty()) try {
		type = std::stoi(text);
		if (type >= Waypoint::unknown && type < Waypoint::numOfWaypointTypes) return true;
	} catch(...) {}
	type = Waypoint::unknown;
	return false;
}

bool SeeYou::ParseRunwayDir(const std::string& text, int& dir) {
	if (text.empty()) {
		dir = 0; // zero means runway direction unknown
		return true;
	}
	try {
		dir = std::stoi(text);
		if (dir > 0 && dir <= 360) return true;
		if (dir == 0) {
			dir = 360;
			return true;
		}
	} catch(...) {}
	dir = 0;
	return false;
}

bool SeeYou::ParseRunwayLength(const std::string& text, int& length) {
	length = 0; // zero means invalid/unknown length
	if (text.empty()) return true; // empty field: declared as unknown
	int pos = (int)text.length() - 1;
	if(pos<2) return false; // at least 3 chars: 2 digit and a letter for the unit: a runway of "9m" (2 char) would be not possible...
	bool nauticalMiles = false, statuteMiles = false;
	if(text.back() == 'm' || text.back() == 'M') {
		if(text.at(pos) == 'n' || text.at(pos) == 'N') {
			pos--;
			nauticalMiles = true;
		}
	} else if((text.at(pos-1) == 'm' || text.at(pos-1) == 'M') && (text.back() == 'l' || text.back() == 'L' || text.back() == 'i' || text.back() == 'I')) {
		pos--;
		statuteMiles = true;
	} else return false; // Unable to parse unit
	try {
		double len = std::stod(text.substr(0,pos));
		if (len < 0) return false;
		if (nauticalMiles) len *= Geometry::NM2M;
		else if (statuteMiles) len *= Geometry::MI2M;
		length = (int)std::round(len);
		return true;
	} catch (...) {}
	return false;
}

bool SeeYou::ParseAirfieldFrequencies(const std::string& text, float& freq, float& secondaryFreq) {
	freq = 0;
	secondaryFreq = 0;
	if (text.empty()) return true;
	try {
		size_t pos;
		freq = std::stof(text,&pos);
		if (!AirspaceConverter::IsValidAirbandFrequency(freq)) { // Invalid main frequency for radio communication
			freq = 0;
			return false;
		}
		if (pos < text.length()) {
			secondaryFreq = std::fabs(std::stof(text.substr(pos)));
			if (!AirspaceConverter::IsValidAirbandFrequency(secondaryFreq)) { // Invalid secondary frequency for radio communication
				secondaryFreq = 0;
				return false;
			}
		}
		return true;
	} catch(...) {}
	return false;
}

bool SeeYou::ParseOtherFrequency(const std::string& text, const int type, float& freq) {
	freq = 0;
	if (text.empty()) return true;
	if (type != Waypoint::WaypointType::VOR && type != Waypoint::WaypointType::NDB) return false; // This waypoint type is not supposed to have a frequency associated
	try {
		freq = std::stof(text);
		if (type == Waypoint::WaypointType::VOR ? AirspaceConverter::IsValidVORfrequency(freq) : AirspaceConverter::IsValidNDBfrequency(freq)) return true;
		else { // Invalid frequency for VOR or DME
			freq = 0;
			return false;
		}
	} catch(...) {}
	return false;
}

bool SeeYou::Read(const std::string& fileName) {
	std::ifstream input(fileName, std::ios::binary);
	if (!input.is_open() || input.bad()) {
		AirspaceConverter::LogMessage("ERROR: Unable to open CUP input file: " + fileName, true);
		return false;
	}
	AirspaceConverter::LogMessage("Reading CUP file: " + fileName, false);
	int linecount = 0;
	std::string sLine;
	bool isCRLF = false, CRLFwarningGiven = false, firstWaypointFound = false;

	double latitude, longitude;
	int type, runwayDir, runwayLength;
	float altitude, radioFreq, altRadioFreq;


	while (!input.eof() && input.good()) {

		// Get the line
		AirspaceConverter::SafeGetline(input, sLine, isCRLF);
		linecount++;

		// Verify line ending
		if (!CRLFwarningGiven && !isCRLF) {
			AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: not valid Windows style end of line (expected CR LF).") % linecount), true);

			// CUP files may contain thousands of WPs we don't want to print this warning all the time
			CRLFwarningGiven = true;
		}

		// Directly skip empty lines
		if (sLine.empty()) continue;

		// Skip eventual header
		if (!firstWaypointFound && (
				sLine.find("name, code, country, lat, lon, elev, style, rwydir, rwylen, freq, desc") != std::string::npos ||
				sLine.find("name,code,country,lat,lon,elev,style,rwdir,rwlen,freq,desc") != std::string::npos)) continue;

		// Remove front spaces
		boost::algorithm::trim_left(sLine);

		// If it was a line with only spaces skip it
		if (sLine.empty()) continue;

		// Then directly skip full comment line
		if (sLine.front() == '*') continue;

		// Remove back spaces
		boost::algorithm::trim_right(sLine);

		// Skip too short lines
		if (sLine.size() <= 10) { // At least ten commas should be there
			AirspaceConverter::LogMessage(boost::str(boost::format("ERROR: line %1d is too short to contain anything useful: %2s") %linecount %sLine), true);
			continue;
		}

		// Check if we arrived to the task section, if yes we're done
		if (sLine == "-----Related Tasks-----") break;

		// Tokenize with quotes
		boost::tokenizer<boost::escaped_list_separator<char> > tokens(sLine); // default separator:',', default quote:'"', default escape char:'\'
		if (std::distance(tokens.begin(),tokens.end()) != 11) { // We expect only 11 fields
			AirspaceConverter::LogMessage(boost::str(boost::format("ERROR on line %1d: expected 11 fields: %2s") %linecount %sLine), true);
			continue;
		}

		// Long name
		boost::tokenizer<boost::escaped_list_separator<char> >::iterator token=tokens.begin();
		const std::string name = *token;
		if (name.empty()) {
			AirspaceConverter::LogMessage(boost::str(boost::format("ERROR on line %1d: a name must be present: %2s") %linecount %sLine), true);
			continue;
		}

		// Code (short name)
		const std::string code = *(++token);

		// Country code
		const std::string country = *(++token);

		// Latitude
		if (!ParseLatitude(*(++token), latitude)) {
			AirspaceConverter::LogMessage(boost::str(boost::format("ERROR on line %1d: invalid latitude: %2s") %linecount %(*token)), true);
			continue;
		}

		// Longitude
		if (!ParseLongitude(*(++token), longitude)) {
			AirspaceConverter::LogMessage(boost::str(boost::format("ERROR on line %1d: invalid longitude: %2s") %linecount %(*token)), true);
			continue;
		}

		// Elevation
		if (!ParseAltitude(*(++token), altitude))
			AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: invalid elevation: %2s, assuming AMSL") %linecount %(*token)), true);

		// Waypoint style
		if (!ParseStyle(*(++token),type))
			AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: invalid waypoint style: %2s, assuming unknown") %linecount %(*token)), true);

		// If it's an airfield...
		if(Waypoint::IsTypeAirfield((Waypoint::WaypointType)type)) {
			// Runway direction
			if (!ParseRunwayDir(*(++token),runwayDir))
				AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: invalid runway direction: %2s") % linecount % (*token)), true);

			// Runway length
			if (!ParseRunwayLength(*(++token),runwayLength))
				AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: invalid runway length: %2s") %linecount %(*token)), true);

			// Radio frequency
			if (!ParseAirfieldFrequencies(*(++token),radioFreq,altRadioFreq)) AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: invalid radio frequency for airfield: %2s") %linecount %(*token)), true);

			// Description
			std::string description = *(++token);
			assert(token != tokens.end());

			// Build the airfield
			Airfield* airfield = new Airfield(name, code, country, latitude, longitude, altitude, type, runwayDir, runwayLength, radioFreq, description);
			if (altRadioFreq > 0) airfield->SetOtherFrequency(altRadioFreq);

			// Add it to the multimap
			waypoints.insert(std::pair<int, Waypoint*>(type, (Waypoint*)airfield));
		} else {
			// Skip runway length and direction
			token++;
			token++;

			// Frequency may be used for VOR and NDB
			if (!ParseOtherFrequency(*(++token), type, radioFreq))
				AirspaceConverter::LogMessage(boost::str(boost::format("WARNING on line %1d: invalid frequency for non airfield waypoint: %2s") %linecount %(*token)), true);

			// Description
			std::string description = *(++token);
			assert(token != tokens.end());

			// Build the waypoint
			Waypoint* waypoint = new Waypoint(name, code, country, latitude, longitude, altitude, type, description);
			if (radioFreq > 0) waypoint->SetOtherFrequency(radioFreq);

			// Add it to the multimap
			waypoints.insert(std::pair<int, Waypoint*>(type, waypoint));
		}

		// Make sure that at this point we already found a valid waypoint so the header is not anymore expected
		if (!firstWaypointFound) firstWaypointFound = true;
	}
	return true;
}


bool SeeYou::Write(const std::string& fileName) {
	if (waypoints.empty()) {
		AirspaceConverter::LogMessage("SeeYou output: no waypoints, nothing to write", false);
		return false;
	}
	std::ofstream file;
	file.open(fileName, std::ios::out | std::ios::trunc | std::ios::binary);
	if (!file.is_open() || file.bad()) {
		AirspaceConverter::LogMessage("ERROR: Unable to open output file: " + fileName, true);
		return false;
	}
	AirspaceConverter::LogMessage("Writing output file: " + fileName, false);

	// Write default CUP header on first line
	file << "name, code, country, lat, lon, elev, style, rwydir, rwylen, freq, desc\r\n\r\n";

	// Write our disclaimer
	for(const std::string& line: AirspaceConverter::disclaimer) file << "* " << line << "\r\n";

	// Write creation date
	file << "\r\n* " << AirspaceConverter::GetCreationDateString() << "\r\n\r\n";

	// Go trough all waypoints
	for (const std::pair<int,Waypoint*>& pair : waypoints) {
		assert(pair.second != nullptr);
		const Waypoint& w(*pair.second);

		// Name is mandatory according to SeeYou specs
		if (w.GetName().empty()) {
			AirspaceConverter::LogMessage("Warning: skipping waypoint with long name empty: " + w.GetCode(), true);
			continue;
		}

		// Long name
		file << '"' << w.GetName() << "\",";

		// Code "short name"
		if (!w.GetCode().empty()) file << '"' << w.GetCode() << '"';

		// Country code
		file << ',' << w.GetCountry() << ',';

		// Latitude
		int deg;
		double min; // the minutes must be expressed with 3 decimals so they have to be rounded on three decimals
		const Geometry::LatLon& pos(w.GetPosition());
		pos.GetLatDegMin(deg,min);
		file << std::setw(2) << std::setfill('0') << deg << std::setw(6) << std::setfill('0') << std::fixed << std::setprecision(3) << std::round(min*1000)/1000 << pos.GetNorS() << ',';

		// Longitude
		pos.GetLonDegMin(deg,min);
		file << std::setw(3) << std::setfill('0') << deg << std::setw(6) << std::setfill('0') << std::fixed << std::setprecision(3) << std::round(min*1000)/1000 << pos.GetEorW() << ',';

		// Altitude
		if (w.GetAltitude() != 0) file << std::fixed << std::setprecision(1) << std::round(w.GetAltitude()*10)/10 << "m,"; // round altitude in meters on one decimal
		else file << "0,";

		// Waypoint style
		file << (int)w.GetType() << ',';

		if (w.IsAirfield()) {
			const Airfield& a((const Airfield&)w);

			// Runway direction
			if (a.HasRunwayDir()) file << std::setw(3) << std::setfill('0') << a.GetRunwayDir();
			file << ',';

			// Runway length
			if (a.HasRunwayLength()) file << a.GetRunwayLength() << 'm';
			file << ',';

			// Radio frequency
			if (a.HasRadioFrequency()) {
				file << std::fixed << std::setprecision(3) << a.GetRadioFrequency();
				if (a.HasOtherFrequency()) file << '-' << std::fixed << std::setprecision(3) << a.GetOtherFrequency();
			}
		} else {
			file << ",,"; // Skip runway length and direction

			// Other frequency
			if (w.HasOtherFrequency()) {
				if (w.GetType() == Waypoint::WaypointType::NDB) file << std::fixed << std::setprecision(1) << w.GetOtherFrequency(); // 1 decimal for NDB freq [kHz]
				else if (w.GetType() == Waypoint::WaypointType::VOR) file << std::fixed << std::setprecision(2) << w.GetOtherFrequency(); // 2 decimals for VOR freq [MHz]
				else file << std::fixed << std::setprecision(3) << w.GetOtherFrequency(); // assuming all other VHF freq [MHz]
			}
		}
		file << ',';

		// Description
		if (!w.GetDescription().empty()) file << '"' << w.GetDescription() << '"';

		file << "\r\n";
	}

	file.close();
	return true;
}
