#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */
#include <map>
#include <vector>
#include <string>
#include <stdint.h>

class CVariant
{
public:
  enum VariantType
  {
    VariantTypeInteger,
    VariantTypeUnsignedInteger,
    VariantTypeBoolean,
    VariantTypeString,
    VariantTypeArray,
    VariantTypeObject,
    VariantTypeNull,
    VariantTypeConstNull
  };

  CVariant(VariantType type = VariantTypeNull);
  CVariant(int integer);
  CVariant(int64_t integer);
  CVariant(unsigned int unsignedinteger);
  CVariant(uint64_t unsignedinteger);
  CVariant(bool boolean);
  CVariant(const char *str);
  CVariant(const CVariant &variant);

  ~CVariant();

  bool isInteger() const;
  bool isUnsignedInteger() const;
  bool isBoolean() const;
  bool isString() const;
  bool isArray() const;
  bool isObject() const;
  bool isNull() const;

  int64_t asInteger(int64_t fallback = 0) const;
  uint64_t asUnsignedInteger(uint64_t fallback = 0u) const;
  bool asBoolean(bool fallback = false) const;
  const char *asString(const char *fallback = "") const;

  CVariant &operator[](std::string key);
  CVariant &operator[](unsigned int position);

  CVariant &operator=(const CVariant &rhs);

  void push_back(CVariant variant);

  unsigned int size() const;
  bool empty() const;
  void clear();
  void erase(std::string key);
  void erase(unsigned int position);

  void debug();
  void internaldebug();
private:
  VariantType m_type;

  typedef std::vector<CVariant> VariantArray;
  typedef std::map<std::string, CVariant> VariantMap;

  union
  {
    int64_t integer;
    uint64_t unsignedinteger;
    bool boolean;
    std::string *string;
    VariantArray *array;
    VariantMap *map;
  } m_data;

  static CVariant ConstNullVariant;
};
