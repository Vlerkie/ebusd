/*
 * Copyright (C) John Baier 2014 <ebusd@johnm.de>
 *
 * This file is part of ebusd.
 *
 * ebusd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ebusd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ebusd. If not, see http://www.gnu.org/licenses/.
 */

#include "data.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <math.h>

using namespace std;

static const dataType_t stringDataType = {
	"STR",16*8,bt_str,     ADJ,        ' ',          1,         16,    0, 0  // >= 1 byte character string filled up with space
};

static const dataType_t pinDataType = {
	"PIN", 16, bt_num, FIX|BCD|REV, 0xffff,          0,     0x9999,    1, 0 // unsigned decimal in BCD, 0000 - 9999 (fixed length)
};

static const dataType_t uchDataType = {
	"UCH",  8, bt_num,     LST,       0xff,          0,       0xfe,    1, 0 // unsigned integer, 0 - 254
};

/** the known data field types. */
static const dataType_t dataTypes[] = {
	{"IGN",16*8,bt_str, IGN|ADJ,          0,          1,         16,    0, 0}, // >= 1 byte ignored data
	stringDataType,
	{"HEX",16*8,bt_hexstr,  ADJ,          0,          2,         47,    0, 0}, // >= 1 byte hex digit string, usually separated by space, e.g. 0a 1b 2c 3d
	{"BDA", 32, bt_dat,     BCD,          0,         10,         10,    0, 0}, // date with weekday in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x31,0x12,WW,0x99, WW is weekday Mon=0x00 - Sun=0x06)
	{"BDA", 24, bt_dat,     BCD,          0,         10,         10,    0, 0}, // date in BCD, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x31,0x12,0x99)
	{"HDA", 32, bt_dat,       0,          0,         10,         10,    0, 0}, // date with weekday, 01.01.2000 - 31.12.2099 (0x01,0x01,WW,0x00 - 0x1f,0x0c,WW,0x63, WW is weekday Mon=0x01 - Sun=0x07))
	{"HDA", 24, bt_dat,       0,          0,         10,         10,    0, 0}, // date, 01.01.2000 - 31.12.2099 (0x01,0x01,0x00 - 0x1f,0x0c,0x63)
	{"BTI", 24, bt_tim, BCD|REV,          0,          8,          8,    0, 0}, // time in BCD, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x59,0x59,0x23)
	{"HTI", 24, bt_tim,       0,          0,          8,          8,    0, 0}, // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x17,0x3b,0x3b)
	{"VTI", 24, bt_tim,     REV,       0x63,          8,          8,    0, 0}, // time, 00:00:00 - 23:59:59 (0x00,0x00,0x00 - 0x3b,0x3b,0x17, replacement 0x63) [Vaillant type]
	{"HTM", 16, bt_tim,       0,          0,          5,          5,    0, 0}, // time as hh:mm, 00:00 - 23:59 (0x00,0x00 - 0x17,0x3b)
	{"TTM",  8, bt_tim,       0,       0x90,          5,          5,    0, 0}, // truncated time (only multiple of 10 minutes), 00:00 - 24:00 (minutes div 10 + hour * 6 as integer)
	{"BDY",  8, bt_num, DAY|LST,       0x07,          0,          6,    1, 0}, // weekday, "Mon" - "Sun" (0x00 - 0x06) [ebus type]
	{"HDY",  8, bt_num, DAY|LST,       0x00,          1,          7,    1, 0}, // weekday, "Mon" - "Sun" (0x01 - 0x07) [Vaillant type]
	{"BCD",  8, bt_num, BCD|LST,       0xff,          0,       0x99,    1, 0}, // unsigned decimal in BCD, 0 - 99
	pinDataType,
	uchDataType,
	{"SCH",  8, bt_num,     SIG,       0x80,       0x81,       0x7f,    1, 0}, // signed integer, -127 - +127
	{"D1B",  8, bt_num,     SIG,       0x80,       0x81,       0x7f,    1, 0}, // signed integer, -127 - +127
	{"D1C",  8, bt_num,       0,       0xff,       0x00,       0xc8,    2, 1}, // unsigned number (fraction 1/2), 0 - 100 (0x00 - 0xc8, replacement 0xff)
	{"UIN", 16, bt_num,     LST,     0xffff,          0,     0xfffe,    1, 0}, // unsigned integer, 0 - 65534
	{"SIN", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,    1, 0}, // signed integer, -32767 - +32767
	{"FLT", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff, 1000, 3}, // signed number (fraction 1/1000), -32.767 - +32.767
	{"D2B", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,  256, 3}, // signed number (fraction 1/256), -127.99 - +127.99
	{"D2C", 16, bt_num,     SIG,     0x8000,     0x8001,     0x7fff,   16, 2}, // signed number (fraction 1/16), -2047.9 - +2047.9
	{"ULG", 32, bt_num,     LST, 0xffffffff,          0, 0xfffffffe,    1, 0}, // unsigned integer, 0 - 4294967294
	{"SLG", 32, bt_num,     SIG, 0x80000000, 0x80000001, 0xffffffff,    1, 0}, // signed integer, -2147483647 - +2147483647
	{"BI0",  7, bt_num, ADJ|LST,          0,          0,       0xef,    1, 0}, // bit 0 (up to 7 bits until bit 6)
	{"BI1",  7, bt_num, ADJ|LST,          0,          0,       0x7f,    1, 1}, // bit 1 (up to 7 bits until bit 7)
	{"BI2",  6, bt_num, ADJ|LST,          0,          0,       0x3f,    1, 2}, // bit 2 (up to 6 bits until bit 7)
	{"BI3",  5, bt_num, ADJ|LST,          0,          0,       0x1f,    1, 3}, // bit 3 (up to 5 bits until bit 7)
	{"BI4",  4, bt_num, ADJ|LST,          0,          0,       0x0f,    1, 4}, // bit 4 (up to 4 bits until bit 7)
	{"BI5",  3, bt_num, ADJ|LST,          0,          0,       0x07,    1, 5}, // bit 5 (up to 3 bits until bit 7)
	{"BI6",  2, bt_num, ADJ|LST,          0,          0,       0x03,    1, 6}, // bit 6 (up to 2 bits until bit 7)
	{"BI7",  1, bt_num, ADJ|LST,          0,          0,       0x01,    1, 7}, // bit 7
};


/** the week day names. */
static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

#define MAX_POS 16

unsigned int parseInt(const char* str, int base, const unsigned int minValue, const unsigned int maxValue, result_t& result, unsigned int* length) {
	char* strEnd = NULL;

	unsigned int ret = strtoul(str, &strEnd, base);

	if (strEnd == NULL || *strEnd != 0) {
		result = RESULT_ERR_INVALID_NUM; // invalid value
		return 0;
	}

	if (ret < minValue || ret > maxValue) {
		result = RESULT_ERR_OUT_OF_RANGE; // invalid value
		return 0;
	}
	if (length != NULL)
		*length = strEnd - str;

	result = RESULT_OK;
	return ret;
}

void printErrorPos(vector<string>::iterator begin, const vector<string>::iterator end, vector<string>::iterator pos, string filename, size_t lineNo, result_t result)
{
	if (pos > begin)
		pos--;
	cout << "Error reading \"" << filename << "\" line " << static_cast<unsigned>(lineNo) << " field " << static_cast<unsigned>(1+pos.base()-begin.base()) << " value \"" << *pos << "\": " << getResultCode(result) << endl;
	cout << "Erroneous item is here:" << endl;
	bool first = true;
	int cnt = 0;
	while (begin != end) {
		if (first == true)
			first = false;
		else {
			cout << FIELD_SEPARATOR;
			if (begin <= pos) {
				cnt++;
			}
		}
		if (begin < pos) {
			cnt += 1+(*begin).length()+1;
		} else if (begin == pos) {
			cnt++;
		}
		string item = *begin++;
		cout << TEXT_SEPARATOR << item << TEXT_SEPARATOR;
	}
	cout << endl;
	cout << setw(cnt) << " " << setw(0) << "^" << endl;
}


result_t DataField::create(vector<string>::iterator& it,
		const vector<string>::iterator end,
		DataFieldTemplates* templates,
		DataField*& returnField, const bool isSetMessage,
		const unsigned char dstAddress)
{
	vector<SingleDataField*> fields;
	string firstName, firstComment;
	result_t result = RESULT_OK;
	if (it == end)
		return RESULT_ERR_EOF;

	do {
		string unit, comment;
		PartType partType;
		unsigned int divisor = 0;
		const bool isTemplate = dstAddress == SYN;
		bool hasPartStr = false;
		string token;

		// templates: name,type[:len][,[divisor|values][,[unit][,[comment]]]]
		// normal: name,part,type[:len][,[divisor|values][,[unit][,[comment]]]]
		const string name = *it++;
		if (it == end)
			break;

		if (isTemplate == true)
			partType = pt_any;
		else {
			const char* partStr = (*it++).c_str();
			if (it == end)
				break;

			hasPartStr = partStr[0] != 0;
			if (dstAddress == BROADCAST || isMaster(dstAddress) == true
				|| (isSetMessage == true && hasPartStr == false)
				|| strcasecmp(partStr, "M") == 0) { // master data
				partType = pt_masterData;
			}
			else if ((isSetMessage == false && hasPartStr == false)
				|| strcasecmp(partStr, "S") == 0) { // slave data
				partType = pt_slaveData;
			}
			else {
				result = RESULT_ERR_INVALID_PART;
				break;
			}
		}

		if (fields.empty() == true) {
			firstName = name;
			firstComment = comment;
		}

		string typeStr = *it++;
		if (typeStr.empty() == true) {
			if (name.empty() == false || hasPartStr == true)
				result = RESULT_ERR_MISSING_TYPE;
			break;
		}

		map<unsigned int, string> values;
		if (it != end) {
			string divisorStr = *it++;
			if (divisorStr.empty() == false) {
				if (divisorStr.find('=') == string::npos)
					divisor = parseInt(divisorStr.c_str(), 10, 1, 10000, result);
				else {
					istringstream stream(divisorStr);
					while (getline(stream, token, VALUE_SEPARATOR) != 0) {
						const char* str = token.c_str();
						char* strEnd = NULL;
						unsigned int id = strtoul(str, &strEnd, 10);
						if (strEnd == NULL || strEnd == str || *strEnd != '=') {
							result = RESULT_ERR_INVALID_LIST;
							break;
						}

						values[id] = string(strEnd + 1);
					}
				}
				if (result != RESULT_OK)
					break;
			}
		}

		if (it == end)
			unit = "";
		else {
			unit = *it++;
			if (strcasecmp(unit.c_str(), NULL_VALUE) == 0)
				unit.clear();
		}

		if (it == end)
			comment = "";
		else {
			comment = *it++;
			if (strcasecmp(comment.c_str(), NULL_VALUE) == 0)
				comment.clear();
		}

		size_t pos = typeStr.find(LENGTH_SEPARATOR);
		unsigned char length;
		if (pos == string::npos) {
			length = 0;
			// check for reference(s) to templates
			if (templates != NULL) {
				istringstream stream(typeStr);
				bool found = false;
				string lengthStr;
				while (result == RESULT_OK && getline(stream, token, VALUE_SEPARATOR) != 0) {
					DataField* templ = templates->get(token);
					if (templ == NULL) {
						if (found == false)
							break; // fallback to direct definition
						result = RESULT_ERR_NOTFOUND; // cannot mix reference and direct definition
					}
					else {
						found = true;
						result = templ->derive("", "", "", partType, divisor, values, fields);
					}
				}
				if (result != RESULT_OK)
					break;
				if (found == true)
					continue; // go to next definition
			}
		}
		else {
			length = parseInt(typeStr.substr(pos+1).c_str(), 10, 1, MAX_POS, result);
			if (result != RESULT_OK)
				break;
			typeStr = typeStr.substr(0, pos);
		}

		SingleDataField* add = NULL;
		const char* typeName = typeStr.c_str();
		for (size_t i = 0; result == RESULT_OK && add == NULL && i < sizeof(dataTypes) / sizeof(dataTypes[0]); i++) {
			dataType_t dataType = dataTypes[i];
			if (strcasecmp(typeName, dataType.name) == 0) {
				unsigned char bitCount = dataType.maxBits;
				unsigned char useLength = (bitCount + 7) / 8;
				if ((dataType.flags & ADJ) != 0) { // adjustable length
					if ((bitCount % 8) != 0) {
						if (length == 0) {
							useLength = 1; // default length: 1 byte
							bitCount = 1; // default count: 1 bit
						}
						else if (length > bitCount) {
							result = RESULT_ERR_OUT_OF_RANGE; // invalid length
							break;
						}
						else {
							bitCount = length;
							useLength = (length + 7) / 8;
						}
					}
					else if (length == 0) {
						useLength = 1; // default length: 1 byte
					}
					else if (length <= useLength) {
						useLength = length;
					}
					else {
						result = RESULT_ERR_OUT_OF_RANGE; // invalid length
						break;
					}
				}
				else if (length > 0 && length != useLength)
					continue; // check for another one with same name but different length

				switch (dataType.type)
				{
				case bt_str:
				case bt_hexstr:
				case bt_dat:
				case bt_tim:
					add = new StringDataField(name, comment, unit, dataType, partType, useLength);
					break;
				case bt_num:
					if (values.empty() == true && (dataType.flags & DAY) != 0) {
						for (unsigned int i = 0; i < sizeof(dayNames) / sizeof(dayNames[0]); i++)
							values[dataType.minValueOrLength + i] = dayNames[i];
					}
					if (values.empty() == true || (dataType.flags & LST) == 0) {
						if (divisor == 0) {
							divisor = dataType.divisor;
						}
						else
							divisor *= dataType.divisor;

						add = new NumberDataField(name, comment, unit, dataType, partType, useLength, bitCount, divisor);
						break;
					}
					if (values.begin()->first < dataType.minValueOrLength
							|| values.rbegin()->first > dataType.maxValueOrLength) {
						result = RESULT_ERR_OUT_OF_RANGE;
						break;
					}
					//TODO add special field for fixed values (exactly one value in the list of values)
					add = new ValueListDataField(name, comment, unit, dataType, partType, useLength, bitCount, values);
					break;
				}
			}
		}
		if (add != NULL)
			fields.push_back(add);
		else if (result == RESULT_OK)
			result = RESULT_ERR_NOTFOUND; // type not found

	} while (it != end && result == RESULT_OK);

	if (fields.empty() == true || result != RESULT_OK) {
		while (fields.empty() == false) { // cleanup already created fields
			delete fields.back();
			fields.pop_back();
		}
		return result == RESULT_OK ? RESULT_ERR_INVALID_ARG : result;
	}

	if (fields.size() == 1)
		returnField = fields[0];
	else {
		returnField = new DataFieldSet(firstName, firstComment, fields);
	}
	return RESULT_OK;
}


void SingleDataField::dump(ostream& output)
{
	output << m_name << FIELD_SEPARATOR;
	if (m_partType == pt_masterData)
		output << "m";
	else if (m_partType == pt_slaveData)
		output << "s";
	output << FIELD_SEPARATOR << m_dataType.name;
}

result_t SingleDataField::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		ostringstream& output, bool leadingSeparator,
		bool verbose, const char* filterName,
		char separator)
{
	if (partType != m_partType)
		return RESULT_OK;

	switch (m_partType)
	{
	case pt_masterData:
		offset += 5; // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset += 1; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	if (isIgnored() == true || (filterName != NULL && m_name != filterName)) {
		if (offset + m_length > data.size()) {
			return RESULT_ERR_INVALID_POS;
		}
		return RESULT_EMPTY;
	}

	if (leadingSeparator == true)
		output << separator;

	if (verbose == true)
		output << m_name << "=";

	result_t result = readSymbols(data, offset, output);
	if (result != RESULT_OK)
		return result;

	if (verbose && m_unit.length() > 0)
		output << " " << m_unit;
	if (verbose && m_comment.length() > 0)
		output << " [" << m_comment << "]";

	return RESULT_OK;
}

result_t SingleDataField::write(istringstream& input,
		const PartType partType, SymbolString& data,
		unsigned char offset, char separator)
{
	if (partType != m_partType)
		return RESULT_OK;

	switch (m_partType)
	{
	case pt_masterData:
		offset += 5; // skip QQ ZZ PB SB NN
		break;
	case pt_slaveData:
		offset += 1; // skip NN
		break;
	default:
		return RESULT_ERR_INVALID_PART;
	}
	return writeSymbols(input, offset, data);
}


result_t StringDataField::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (divisor != 0 || values.empty() == false)
		return RESULT_ERR_INVALID_ARG; // cannot set divisor or values for string field
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;

	fields.push_back(new StringDataField(name, comment, unit, m_dataType, partType, m_length));

	return RESULT_OK;
}

void StringDataField::dump(ostream& output)
{
	SingleDataField::dump(output);
	if ((m_dataType.flags & ADJ) != 0)
		output << ":" << static_cast<unsigned>(m_length);
	output << FIELD_SEPARATOR << FIELD_SEPARATOR; // no value list, no divisor
	output << m_unit << FIELD_SEPARATOR << m_comment << FIELD_SEPARATOR;
}

result_t StringDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, ostringstream& output)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch, last = 0;

	if (baseOffset + m_length > input.size()) {
		return RESULT_ERR_INVALID_POS;
	}

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
		if (m_length == 4 && i == 2 && m_dataType.type == bt_dat)
			continue; // skip weekday in between
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0) {
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_OUT_OF_RANGE; // invalid BCD
			ch = (ch >> 4) * 10 + (ch & 0x0f);
		}
		switch (m_dataType.type)
		{
		case bt_hexstr:
			if (i > 0)
				output << ' ';
			output << setw(2) << hex << setfill('0') << static_cast<unsigned>(ch);
			break;
		case bt_dat:
			if (i + 1 == m_length)
				output << (2000 + ch);
			else if (ch < 1 || (i == 0 && ch > 31) || (i == 1 && ch > 12))
				return RESULT_ERR_OUT_OF_RANGE; // invalid date
			else
				output << setw(2) << dec << setfill('0') << static_cast<unsigned>(ch) << ".";
			break;
		case bt_tim:
			if (m_dataType.replacement != 0 && ch == m_dataType.replacement) {
				if (m_length == 1) { // truncated time
					output << NULL_VALUE << ":" << NULL_VALUE;
					break;
				}
				if (i > 0)
					output << ":";
				output << NULL_VALUE;
				break;
			}
			if (m_length == 1) { // truncated time
				if (i == 0) {
					ch /= 6; // hours
					offset -= incr; // repeat for minutes
					count++;
				}
				else
					ch = (ch % 6) * 10; // minutes
			}
			if ((i == 0 && ch > 24) || (i > 0 && (ch > 59 || (last == 24 && ch > 0) )))
				return RESULT_ERR_OUT_OF_RANGE; // invalid time
			if (i > 0)
				output << ":";
			output << setw(2) << dec << setfill('0') << static_cast<unsigned>(ch);
			break;
		default:
			if (ch < 0x20)
				ch = m_dataType.replacement;
			output << setw(0) << dec << static_cast<char>(ch);
			break;
		}
		last = ch;
	}

	return RESULT_OK;
}

result_t StringDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned long int value = 0, last = 0, lastLast = 0;
	string token;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	if (isIgnored() == true) {
		for (size_t offset = start, i = 0; i < count; offset += incr, i++) {
			output[baseOffset + offset] = m_dataType.replacement; // fill up with replacement
		}
		return RESULT_OK;
	}
	result_t result;
	size_t i = 0;
	for (size_t offset = start; i < count; offset += incr, i++) {
		switch (m_dataType.type)
		{
		case bt_hexstr:
			while (input.eof() == false && input.peek() == ' ')
				input.get();
			if (input.eof() == true) // no more digits
				value = m_dataType.replacement; // fill up with replacement
			else {
				token.clear();
				token.push_back(input.get());
				if (input.eof() == true)
					return RESULT_ERR_INVALID_NUM; // too short hex value
				token.push_back(input.get());
				if (input.eof() == true)
					return RESULT_ERR_INVALID_NUM; // too short hex value

				value = parseInt(token.c_str(), 16, 0, 0xff, result);
				if (result != RESULT_OK)
					return result; // invalid hex value
			}
			break;
		case bt_dat:
			if (m_length == 4 && i == 2)
				continue; // skip weekday in between
			if (input.eof() == true || getline(input, token, '.') == 0)
				return RESULT_ERR_EOF; // incomplete
			value = parseInt(token.c_str(), 10, 0, 2099, result);
			if (result != RESULT_OK)
				return result; // invalid date part
			if (i + 1 == m_length) {
				if (m_length == 4) {
					// calculate local week day
					struct tm t;
					t.tm_min = t.tm_sec = 0;
					t.tm_hour = 12;
					t.tm_mday = lastLast;
					t.tm_mon = last-1; // January=0
					t.tm_year = (value < 100 ? value + 2000 : value) - 1900;
					t.tm_isdst = 0; // automatic
					if (mktime(&t) < 0)
						return RESULT_ERR_INVALID_NUM; // invalid date
					unsigned char daysSinceSunday = (unsigned char)t.tm_wday; // Sun=0
					if ((m_dataType.flags & BCD) != 0)
						output[baseOffset + offset - incr] = (6+daysSinceSunday) % 7; // Sun=0x06
					else
						output[baseOffset + offset - incr] = (daysSinceSunday==0 ? 7 : daysSinceSunday); // Sun=0x07
				}
				if (value >= 2000)
					value -= 2000;
				else if (value > 99)
					return RESULT_ERR_OUT_OF_RANGE; // invalid year
			} else if (value < 1 || (i == 0 && value > 31) || (i == 1 && value > 12))
				return RESULT_ERR_OUT_OF_RANGE; // invalid date part
			break;
		case bt_tim:
			if (input.eof() == true || getline(input, token, LENGTH_SEPARATOR) == 0)
				return RESULT_ERR_EOF; // incomplete
			if (m_dataType.replacement != 0 && strcmp(token.c_str(), NULL_VALUE) == 0) {
				value = m_dataType.replacement;
				if (m_length == 1) { // truncated time
					if (i == 0) {
						last = value;
						offset -= incr; // repeat for minutes
						count++;
						continue;
					}
					if (last != m_dataType.replacement)
						return RESULT_ERR_INVALID_NUM; // invalid truncated time minutes
				}
				break;
			}
			value = parseInt(token.c_str(), 10, 0, 59, result);
			if (result != RESULT_OK)
				return result; // invalid time part
			if ((i == 0 && value > 24) || (i > 0 && (last == 24 && value > 0) ))
				return RESULT_ERR_OUT_OF_RANGE; // invalid time part
			if (m_length == 1) { // truncated time
				if (i == 0) {
					last = value;
					offset -= incr; // repeat for minutes
					count++;
					continue;
				}
				if ((value % 10) != 0)
					return RESULT_ERR_INVALID_NUM; // invalid truncated time minutes
				value = last * 6 + (value / 10);
				if (value > 24 * 6)
					return RESULT_ERR_OUT_OF_RANGE; // invalid time
			}
			break;
		default:
			if (input.eof() == true)
				value = m_dataType.replacement;
			else {
				value = input.get();
				if (input.eof() == true || value < 0x20)
					value = m_dataType.replacement;
			}
			break;
		}
		lastLast = last;
		last = value;
		if ((m_dataType.flags & BCD) != 0) {
			if (value > 99)
				return RESULT_ERR_OUT_OF_RANGE; // invalid BCD
			value = ((value / 10) << 4) | (value % 10);
		}
		if (value > 0xff)
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
		output[baseOffset + offset] = (unsigned char)value;
	}

	if (i < m_length)
		return RESULT_ERR_EOF; // input too short

	return RESULT_OK;
}


bool NumericDataField::hasFullByteOffset(bool after)
{
	return m_length > 1 || (m_bitCount % 8) == 0
		|| (after == true && m_bitOffset + (m_bitCount % 8) >= 8);
}

void NumericDataField::dump(ostream& output)
{
	SingleDataField::dump(output);
	if ((m_dataType.flags & ADJ) != 0) {
		if ((m_dataType.maxBits % 8) != 0)
			output << ":" << static_cast<unsigned>(m_bitCount);
		else
			output << ":" << static_cast<unsigned>(m_length);
	}
	output << FIELD_SEPARATOR;
}

result_t NumericDataField::readRawValue(SymbolString& input,
		unsigned char baseOffset, unsigned int& value)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch;

	if (baseOffset + m_length > input.size())
		return RESULT_ERR_INVALID_POS; // not enough data available

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	value = 0;
	for (size_t offset = start, i = 0, exp = 1; i < count; offset += incr, i++) {
		ch = input[baseOffset + offset];
		if ((m_dataType.flags & BCD) != 0) {
			if (ch == m_dataType.replacement) {
				value = m_dataType.replacement;
				return RESULT_OK;
			}
			if ((ch & 0xf0) > 0x90 || (ch & 0x0f) > 0x09)
				return RESULT_ERR_OUT_OF_RANGE; // invalid BCD

			ch = (ch >> 4) * 10 + (ch & 0x0f);
			value += ch * exp;
			exp *= 100;
		}
		else {
			value |= ch * exp;
			exp <<= 8;
		}
	}

	if ((m_dataType.flags & BCD) == 0) {
		value >>= m_bitOffset;
		if ((m_bitCount % 8) != 0)
			value &= (1 << m_bitCount) - 1;
	}

	return RESULT_OK;
}

result_t NumericDataField::writeRawValue(unsigned int value,
		unsigned char baseOffset, SymbolString& output)
{
	size_t start = 0, count = m_length;
	int incr = 1;
	unsigned char ch;

	if ((m_dataType.flags & REV) != 0) { // reverted binary representation (most significant byte first)
		start = m_length - 1;
		incr = -1;
	}

	if ((m_dataType.flags & BCD) == 0) {
		if ((m_bitCount % 8) != 0 && (value & ~((1 << m_bitCount) - 1)) != 0)
			return RESULT_ERR_OUT_OF_RANGE;

		value <<= m_bitOffset;
	}
	for (size_t offset = start, i = 0, exp = 1; i < count; offset += incr, i++) {
		if ((m_dataType.flags & BCD) != 0) {
			if (value == m_dataType.replacement)
				ch = m_dataType.replacement;
			else {
				ch = (value / exp) % 100;
				ch = ((ch / 10) << 4) | (ch % 10);
			}
			exp = exp * 100;
		}
		else {
			ch = (value / exp) & 0xff;
			exp = exp << 8;
		}
		if (offset == start && (m_bitCount % 8) != 0 && baseOffset + offset < output.size())
			output[baseOffset + offset] |= ch;
		else
			output[baseOffset + offset] = ch;
	}

	return RESULT_OK;
}


result_t NumberDataField::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	if (divisor == 0)
		divisor = m_divisor;
	else
		divisor *= m_dataType.divisor;
	if (values.empty() == false) {
		if (divisor != 1)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

		fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, values));
	}
	else
		fields.push_back(new NumberDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, divisor));

	return RESULT_OK;
}

void NumberDataField::dump(ostream& output)
{
	NumericDataField::dump(output);
	output << static_cast<unsigned>(m_divisor) << FIELD_SEPARATOR;
	output << m_unit << FIELD_SEPARATOR << m_comment << FIELD_SEPARATOR;
}

result_t NumberDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, ostringstream& output)
{
	unsigned int value = 0;
	int signedValue;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	output << setw(0) << dec; // initialize output

	if (value == m_dataType.replacement) {
		output << NULL_VALUE;
		return RESULT_OK;
	}

	bool negative = (m_dataType.flags & SIG) != 0 && (value & (1 << (m_bitCount - 1))) != 0;
	if (m_bitCount == 32) {
		if (negative == false) {
			if (m_divisor <= 1)
				output << static_cast<unsigned>(value);
			else
				output << setprecision((m_bitCount % 8) == 0 ? m_dataType.precisionOrFirstBit : 0)
				       << fixed << static_cast<float>(value / (float) m_divisor);
			return RESULT_OK;
		}
		signedValue = (int) value; // negative signed value
	}
	else if (negative) // negative signed value
		signedValue = (int) value - (1 << m_bitCount);
	else
		signedValue = (int) value;

	if (m_divisor <= 1) {
		if ((m_dataType.flags & (FIX|BCD)) == (FIX|BCD))
			output << setw(m_length * 2) << setfill('0');
		output << static_cast<int>(signedValue) << setw(0);
	} else
		output << setprecision((m_bitCount % 8) == 0 ? m_dataType.precisionOrFirstBit : 0)
		       << fixed << static_cast<float>(signedValue / (float) m_divisor);

	return RESULT_OK;
}

result_t NumberDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	unsigned int value;

	const char* str = input.str().c_str();
	if (isIgnored() == true || strcasecmp(str, NULL_VALUE) == 0)
		value = m_dataType.replacement; // replacement value
	else if (str == NULL || *str == 0)
		return RESULT_ERR_EOF; // input too short
	else {
		char* strEnd = NULL;
		if (m_divisor <= 1) {
			if ((m_dataType.flags & SIG) != 0) {
				int signedValue = strtol(str, &strEnd, 10);
				if (signedValue < 0 && m_bitCount != 32)
					value = (unsigned int) (signedValue + (1 << m_bitCount));
				else
					value = (unsigned int) signedValue;
			}
			else
				value = strtoul(str, &strEnd, 10);
			if (strEnd == NULL || *strEnd != 0)
				return RESULT_ERR_INVALID_NUM; // invalid value
		}
		else {
			char* strEnd = NULL;
			double dvalue = strtod(str, &strEnd);
			if (strEnd == NULL || *strEnd != 0)
				return RESULT_ERR_INVALID_NUM; // invalid value
			dvalue = round(dvalue * m_divisor);
			if ((m_dataType.flags & SIG) != 0) {
				if (dvalue < -(1LL << (8 * m_length)) || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
				if (dvalue < 0 && m_bitCount != 32)
					value = (unsigned int) (dvalue + (1 << m_bitCount));
				else
					value = (unsigned int) dvalue;
			}
			else {
				if (dvalue < 0.0 || dvalue >= (1LL << (8 * m_length)))
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
				value = (unsigned int) dvalue;
			}
		}

		if ((m_dataType.flags & SIG) != 0) { // signed value
			if ((value & (1 << (m_bitCount - 1))) != 0) { // negative signed value
				if (value < m_dataType.minValueOrLength)
					return RESULT_ERR_OUT_OF_RANGE; // value out of range
			}
			else if (value > m_dataType.maxValueOrLength)
				return RESULT_ERR_OUT_OF_RANGE; // value out of range
		}
		else if (value < m_dataType.minValueOrLength || value > m_dataType.maxValueOrLength)
			return RESULT_ERR_OUT_OF_RANGE; // value out of range
	}

	return writeRawValue(value, baseOffset, output);
}


result_t ValueListDataField::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (m_partType != pt_any && partType == pt_any)
		return RESULT_ERR_INVALID_PART; // cannot create a template from a concrete instance
	if (name.empty() == true)
		name = m_name;
	if (comment.empty() == true)
		comment = m_comment;
	if (unit.empty() == true)
		unit = m_unit;
	if (divisor != 0 && divisor != 1)
		return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field

	if (values.empty() == false) {
		if (values.begin()->first < m_dataType.minValueOrLength
			|| values.rbegin()->first > m_dataType.maxValueOrLength)
			return RESULT_ERR_INVALID_ARG; // cannot use divisor != 1 for value list field
	}
	else
		values = m_values;

	fields.push_back(new ValueListDataField(name, comment, unit, m_dataType, partType, m_length, m_bitCount, values));

	return RESULT_OK;
}

void ValueListDataField::dump(ostream& output)
{
	NumericDataField::dump(output);
	bool first = true;
	for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++) {
		if (first == true)
			first = false;
		else
			output << VALUE_SEPARATOR;
		output << static_cast<unsigned>(it->first) << "=" << it->second;
	}
	output << FIELD_SEPARATOR;
	output << m_unit << FIELD_SEPARATOR << m_comment << FIELD_SEPARATOR;
}

result_t ValueListDataField::readSymbols(SymbolString& input,
		unsigned char baseOffset, ostringstream& output)
{
	unsigned int value = 0;

	result_t result = readRawValue(input, baseOffset, value);
	if (result != RESULT_OK)
		return result;

	output << setw(0) << dec; // initialize output

	map<unsigned int, string>::iterator it = m_values.find(value);
	if (it != m_values.end()) {
		output << it->second;
		return RESULT_OK;
	}

	if (value == m_dataType.replacement) {
		output << NULL_VALUE;
		return RESULT_OK;
	}

	return RESULT_ERR_NOTFOUND; // value assignment not found
}

result_t ValueListDataField::writeSymbols(istringstream& input,
		unsigned char baseOffset, SymbolString& output)
{
	if (isIgnored() == true)
		return writeRawValue(m_dataType.replacement, baseOffset, output); // replacement value

	const char* str = input.str().c_str();

	for (map<unsigned int, string>::iterator it = m_values.begin(); it != m_values.end(); it++)
		if (it->second.compare(str) == 0)
			return writeRawValue(it->first, baseOffset, output);

	if (strcasecmp(str, NULL_VALUE) == 0)
		return writeRawValue(m_dataType.replacement, baseOffset, output); // replacement value

	return RESULT_ERR_NOTFOUND; // value assignment not found
}


DataFieldSet* DataFieldSet::createIdentFields()
{
	vector<SingleDataField*> fields;
	map<unsigned int, string> manufacturers;
	manufacturers[0x06] = "Karl Dungs GmbH";
	manufacturers[0x0f] = "FH Braunschweig/Wolfenbüttel";
	manufacturers[0x10] = "TEM AG für Elektronik Intertem Vertriebs AG";
	manufacturers[0x11] = "Lamberti Elektronik";
	manufacturers[0x14] = "CEB Compagnie Européenne de Brûleurs S.A.";
	manufacturers[0x15] = "Landis & Staefa";
	manufacturers[0x16] = "FERRO Wärmetechnik GmbH & Co.KG";
	manufacturers[0x17] = "MONDIAL electronic Ges.mbH";
	manufacturers[0x18] = "Wikon Kommunikationstechnik GmbH";
	manufacturers[0x19] = "Wolf GmbH";
	manufacturers[0x20] = "RAWE Electronic GmbH";
	manufacturers[0x30] = "Satronic AG";
	manufacturers[0x40] = "ENCON Electronics";
	manufacturers[0x50] = "G. Kromschröder AG";
	manufacturers[0x60] = "Eberle Controls GmbH";
	manufacturers[0x65] = "EBV Elektronikbau";
	manufacturers[0x75] = "Grässlin GmbH & Co.KG";
	manufacturers[0x85] = "Motoren und Ventilatoren Landshut GmbH";
	manufacturers[0x95] = "SIG Berger Lahr GmbH & Co KG";
	manufacturers[0xa5] = "Theben Zeitschaltautomatik";
	manufacturers[0xa7] = "Thermowatt s.p.a.";
	manufacturers[0xb5] = "Joh. Vaillant GmbH & Co.";
	manufacturers[0xc0] = "Toby AG";
	manufacturers[0xc5] = "Max Weishaupt GmbH";
	fields.push_back(new ValueListDataField("manufacturer", "", "", uchDataType, pt_slaveData, 1, 8, manufacturers));
	fields.push_back(new StringDataField("id", "", "", stringDataType, pt_slaveData, 5));
	fields.push_back(new NumberDataField("software", "", "", pinDataType, pt_slaveData, 2, 16, 0));
	fields.push_back(new NumberDataField("hardware", "", "", pinDataType, pt_slaveData, 2, 16, 0));
	return new DataFieldSet("ident", "", fields);
}

DataFieldSet::~DataFieldSet()
{
	while (m_fields.empty() == false) {
		delete m_fields.back();
		m_fields.pop_back();
	}
}

unsigned char DataFieldSet::getLength(PartType partType)
{
	unsigned char length = 0;

	bool previousFullByteOffset[] = { true, true, true, true };

	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (field->getPartType() == partType) {
			if (previousFullByteOffset[partType] == false && field->hasFullByteOffset(false) == false)
				length--;

			length += field->getLength(partType);

			previousFullByteOffset[partType] = field->hasFullByteOffset(true);
		}
	}

	return length;
}

result_t DataFieldSet::derive(string name, string comment,
		string unit, const PartType partType,
		unsigned int divisor, map<unsigned int, string> values,
		vector<SingleDataField*>& fields)
{
	if (values.empty() == false)
		return RESULT_ERR_INVALID_ARG; // value list not allowed in set derive

	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		result_t result = (*it)->derive("", "", "", partType,  divisor, values, fields);
		if (result != RESULT_OK)
			return result;
	}

	return RESULT_OK;
}

void DataFieldSet::dump(ostream& output)
{
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++)
		(*it)->dump(output);
}

result_t DataFieldSet::read(const PartType partType,
		SymbolString& data, unsigned char offset,
		ostringstream& output, bool leadingSeparator,
		bool verbose, const char* filterName,
		char separator)
{
	bool previousFullByteOffset = true, found = false;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType)
			continue;

		if (previousFullByteOffset == false && field->hasFullByteOffset(false) == false)
			offset--;

		result_t result = field->read(partType, data, offset, output, leadingSeparator, verbose, filterName, separator);

		if (result < RESULT_OK)
			return result;

		offset += field->getLength(partType);
		previousFullByteOffset = field->hasFullByteOffset(true);
		if (result != RESULT_EMPTY) {
			found = true;
			leadingSeparator = true;
		}
	}

	if (verbose == true) {
		if (m_comment.length() > 0)
			output << " [" << m_comment << "]";
	}

	return found == true ? RESULT_OK : RESULT_EMPTY;
}

result_t DataFieldSet::write(istringstream& input,
		const PartType partType, SymbolString& data,
		unsigned char offset, char separator)
{
	string token;

	bool previousFullByteOffset = true;
	for (vector<SingleDataField*>::iterator it = m_fields.begin(); it < m_fields.end(); it++) {
		SingleDataField* field = *it;
		if (partType != pt_any && field->getPartType() != partType)
			continue;

		if (previousFullByteOffset == false && field->hasFullByteOffset(false) == false)
			offset--;

		result_t result;
		if (m_fields.size() > 1) {
			if (field->isIgnored() == true)
				token.clear();
			else if (getline(input, token, separator) == 0)
				token.clear();

			istringstream single(token);
			result = (*it)->write(single, partType, data, offset, separator);
		}
		else
			result = (*it)->write(input, partType, data, offset, separator);

		if (result != RESULT_OK)
			return result;

		offset += field->getLength(partType);
		previousFullByteOffset = field->hasFullByteOffset(true);
	}

	return RESULT_OK;
}


void DataFieldTemplates::clear()
{
	for (map<string, DataField*>::iterator it=m_fieldsByName.begin(); it!=m_fieldsByName.end(); it++) {
		delete it->second;
		it->second = NULL;
	}
	m_fieldsByName.clear();
}

result_t DataFieldTemplates::add(DataField* field, bool replace)
{
	string name = field->getName();
	map<string, DataField*>::iterator it = m_fieldsByName.find(name);
	if (it != m_fieldsByName.end()) {
		if (replace == false)
			return RESULT_ERR_DUPLICATE; // duplicate key

		delete it->second;
		it->second = field;

		return RESULT_OK;
	}

	m_fieldsByName[name] = field;

	return RESULT_OK;
}

result_t DataFieldTemplates::addFromFile(vector<string>::iterator& begin, const vector<string>::iterator end, void* arg, vector< vector<string> >* defaults, const string& filename, unsigned int lineNo)
{
	DataField* field = NULL;
	result_t result = DataField::create(begin, end, this, field);
	if (result != RESULT_OK)
		return result;

	result = add(field);
	if (result != RESULT_OK)
		delete field;

	return result;
}

DataField* DataFieldTemplates::get(const string name)
{
	map<string, DataField*>::const_iterator ref = m_fieldsByName.find(name);
	if (ref == m_fieldsByName.end())
		return NULL;

	return ref->second;
}
