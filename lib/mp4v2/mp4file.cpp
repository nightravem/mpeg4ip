/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2001.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Dave Mackie		dmackie@cisco.com
 */

#include "mp4common.h"

MP4File::MP4File()
{
	m_pFile = NULL;
	m_pRootAtom = new MP4RootAtom();
	m_verbosity = 0;

	m_numReadBits = 0;
	m_bufReadBits = 0;
	m_numWriteBits = 0;
	m_bufWriteBits = 0;
}

MP4File::~MP4File()
{
	fclose(m_pFile);
}

int MP4File::Open(char* fileName, char* mode)
{
	if (m_pFile) {
		fclose(m_pFile);
	}

	m_pFile = fopen(fileName, mode);
	if (m_pFile == NULL) {
		VERBOSE_ERROR(m_verbosity, 
			fprintf(stderr, "Open: failed: %s\n", strerror(errno)));
		return -1;
	}

	return 0;
}

int MP4File::Read()
{
	// TBD on file read, do we destroy previous info?

	try {
		u_int64_t fileSize = GetSize();

		m_pRootAtom->SetFile(this);
		m_pRootAtom->SetStart(0);
		m_pRootAtom->SetSize(fileSize);
		m_pRootAtom->SetEnd(fileSize);

		m_pRootAtom->Read();
	}
	catch (MP4Error* e) {
		VERBOSE_ERROR(m_verbosity, e->Print());
		return -1;
	}

	return 0;
}

int MP4File::Write()
{
	try {
		m_pRootAtom->Write();
	}
	catch (MP4Error* e) {
		VERBOSE_ERROR(m_verbosity, e->Print());
		return -1;
	}

	return 0;
}

int MP4File::Dump(FILE* pDumpFile)
{
	if (pDumpFile == NULL) {
		pDumpFile = stdout;
	}

	try {
		fprintf(pDumpFile, "Dumping meta-information...\n");
		m_pRootAtom->Dump(pDumpFile);
	}
	catch (MP4Error* e) {
		VERBOSE_ERROR(m_verbosity, e->Print());
		return -1;
	}

	return 0;
}

u_int64_t MP4File::GetPosition()
{
	fpos_t fpos;
	if (fgetpos(m_pFile, &fpos) < 0) {
		throw new MP4Error(errno, "MP4GetPosition");
	}
	return fpos.__pos;
}

void MP4File::SetPosition(u_int64_t pos)
{
	fpos_t fpos;
	fpos.__pos = pos;
	if (fsetpos(m_pFile, &fpos) < 0) {
		throw new MP4Error(errno, "MP4SetPosition");
	}
}

u_int64_t MP4File::GetSize()
{
	struct stat s;
	if (fstat(fileno(m_pFile), &s) < 0) {
		throw new MP4Error(errno, "MP4GetSize");
	}
	return s.st_size;
}


u_int32_t MP4File::ReadBytes(u_int8_t* pBytes, u_int32_t numBytes)
{
	ASSERT(m_pFile);
	ASSERT(pBytes);
	ASSERT(numBytes);
	WARNING(m_numReadBits > 0);

	u_int32_t rc;
	rc = fread(pBytes, 1, numBytes, m_pFile);
	if (rc != numBytes) {
		throw new MP4Error(errno, "MP4ReadBytes");
	}
	return rc;
}

u_int32_t MP4File::PeekBytes(u_int8_t* pBytes, u_int32_t numBytes)
{
	u_int64_t pos = GetPosition();
	ReadBytes(pBytes, numBytes);
	SetPosition(pos);
	return numBytes;
}

void MP4File::WriteBytes(u_int8_t* pBytes, u_int32_t numBytes)
{
	ASSERT(m_pFile);
	WARNING(pBytes == NULL);
	WARNING(numBytes == 0);
	WARNING(m_numWriteBits > 0 && m_numWriteBits < 8);

	if (pBytes == NULL || numBytes == 0) {
		return;
	}

	u_int32_t rc;
	rc = fwrite(pBytes, 1, numBytes, m_pFile);
	if (rc != numBytes) {
		throw new MP4Error(errno, "MP4WriteBytes");
	}
}

u_int64_t MP4File::ReadUInt(u_int8_t size)
{
	switch (size) {
	case 1:
		return ReadUInt8();
	case 2:
		return ReadUInt16();
	case 3:
		return ReadUInt24();
	case 4:
		return ReadUInt32();
	case 8:
		return ReadUInt64();
	default:
		ASSERT(false);
		return 0;
	}
}

void MP4File::WriteUInt(u_int64_t value, u_int8_t size)
{
	switch (size) {
	case 1:
		WriteUInt8(value);
	case 2:
		WriteUInt16(value);
	case 3:
		WriteUInt24(value);
	case 4:
		WriteUInt32(value);
	case 8:
		WriteUInt64(value);
	default:
		ASSERT(false);
	}
}

u_int8_t MP4File::ReadUInt8()
{
	u_int8_t data;
	ReadBytes(&data, 1);
	return data;
}

void MP4File::WriteUInt8(u_int8_t value)
{
	WriteBytes(&value, 1);
}

u_int16_t MP4File::ReadUInt16()
{
	u_int8_t data[2];
	ReadBytes(&data[0], 2);
	return ((data[0] << 8) | data[1]);
}

void MP4File::WriteUInt16(u_int16_t value)
{
	u_int8_t data[2];
	data[0] = (value >> 8) & 0xFF;
	data[1] = value & 0xFF;
	WriteBytes(data, 2);
}

u_int32_t MP4File::ReadUInt24()
{
	u_int8_t data[3];
	ReadBytes(&data[0], 3);
	return ((data[0] << 16) | (data[1] << 8) | data[2]);
}

void MP4File::WriteUInt24(u_int32_t value)
{
	u_int8_t data[3];
	data[0] = (value >> 16) & 0xFF;
	data[1] = (value >> 8) & 0xFF;
	data[2] = value & 0xFF;
	WriteBytes(data, 3);
}

u_int32_t MP4File::ReadUInt32()
{
	u_int8_t data[4];
	ReadBytes(&data[0], 4);
	return ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
}

void MP4File::WriteUInt32(u_int32_t value)
{
	u_int8_t data[4];
	data[0] = (value >> 24) & 0xFF;
	data[1] = (value >> 16) & 0xFF;
	data[2] = (value >> 8) & 0xFF;
	data[3] = value & 0xFF;
	WriteBytes(data, 4);
}

u_int64_t MP4File::ReadUInt64()
{
	u_int8_t data[8];
	u_int64_t result = 0;
	u_int64_t temp;

	ReadBytes(&data[0], 8);
	
	for (int i = 0; i < 8; i++) {
		temp = data[i];
		result |= temp << ((7 - i) * 8);
	}
	return result;
}

void MP4File::WriteUInt64(u_int64_t value)
{
	u_int8_t data[8];

	for (int i = 7; i >= 0; i--) {
		data[i] = value & 0xFF;
		value >>= 8;
	}
	WriteBytes(data, 8);
}

float MP4File::ReadFixed16()
{
	u_int8_t iPart = ReadUInt8();
	u_int8_t fPart = ReadUInt8();

	return iPart + (((float)fPart) / 0x100);
}

void MP4File::WriteFixed16(float value)
{
	if (value >= 0x100) {
		throw new MP4Error(ERANGE, "MP4WriteFixed16");
	}

	u_int8_t iPart = (u_int8_t)value;
	u_int8_t fPart = (u_int8_t)((value - iPart) * 0x100);

	WriteUInt8(iPart);
	WriteUInt8(fPart);
}

float MP4File::ReadFixed32()
{
	u_int16_t iPart = ReadUInt16();
	u_int16_t fPart = ReadUInt16();

	return iPart + (((float)fPart) / 0x10000);
}

void MP4File::WriteFixed32(float value)
{
	if (value >= 0x10000) {
		throw new MP4Error(ERANGE, "MP4WriteFixed32");
	}

	u_int16_t iPart = (u_int16_t)value;
	u_int16_t fPart = (u_int16_t)((value - iPart) * 0x10000);

	WriteUInt16(iPart);
	WriteUInt16(fPart);
}

char* MP4File::ReadPascalString()
{
	u_int8_t length = ReadUInt8();
	char* data = (char*)MP4Malloc(length + 1);
	ReadBytes((u_int8_t*)data, length);
	data[length] = '\0';
	return data;
}

void MP4File::WritePascalString(char* string)
{
	u_int32_t length = strlen(string);
	if (length > 255) {
		throw new MP4Error(ERANGE, "MP4WritePascalString");
	}
	WriteUInt8(length);
	WriteBytes((u_int8_t*)string, length);
}

char* MP4File::ReadCString()
{
	u_int32_t length = 0;
	u_int32_t alloced = 256;
	char* data = (char*)MP4Malloc(alloced);

	do {
		if (length == alloced) {
			data = (char*)MP4Realloc(data, alloced * 2);
		}
		ReadBytes((u_int8_t*)&data[length], 1);
		length++;
	} while (data[length - 1] != 0);

	data = (char*)MP4Realloc(data, length - 1);
	return data;
}

void MP4File::WriteCString(char* string)
{
	WriteBytes((u_int8_t*)string, strlen(string) + 1);
}

u_int64_t MP4File::ReadBits(u_int8_t numBits)
{
	ASSERT(numBits > 0);
	ASSERT(numBits <= 64);

	u_int64_t bits = 0;

	for (u_int8_t i = numBits; i > 0; i--) {
		if (m_numReadBits == 0) {
			ReadBytes(&m_bufReadBits, 1);
			m_numReadBits = 8;
		}
		bits = (bits << 1) | ((m_bufReadBits >> (--m_numReadBits)) & 1);
	}

	return bits;
}

void MP4File::WriteBits(u_int64_t bits, u_int8_t numBits)
{
	ASSERT(numBits <= 64);

	for (u_int8_t i = numBits; i > 0; i--) {
		m_bufWriteBits |= ((bits >> (i - 1)) & 1) << (8 - m_numWriteBits++);
		if (m_numWriteBits == 8) {
			FlushWriteBits();
		}
	}
}

void MP4File::FlushWriteBits()
{
	if (m_numWriteBits > 0) {
		WriteBytes(&m_bufWriteBits, 1);
		m_numWriteBits = 0;
		m_bufWriteBits = 0;
	}
}

u_int32_t MP4File::ReadMpegLength()
{
	u_int32_t length = 0;
	u_int8_t temp;
	u_int8_t numBytes = 0;

	do {
		length <<= 7;
		temp = ReadUInt8();
		length |= temp & 0x7F;
		numBytes++;
	} while ((temp & 0x80) && numBytes < 4);

	return length;
}

void MP4File::WriteMpegLength(u_int32_t value, bool compact)
{
	if (value > 0x0FFFFFFF) {
		throw new MP4Error(ERANGE, "MP4WriteMpegLength");
	}

	int8_t numBytes;

	if (compact) {
		if (value <= 0x7F) {
			numBytes = 1;
		} else if (value <= 0x3FFF) {
			numBytes = 2;
		} else if (value <= 0x1FFFFF) {
			numBytes = 3;
		} else {
			numBytes = 4;
		}
	} else {
		numBytes = 4;
	}

	int8_t i = numBytes;
	do {
		i--;
		u_int8_t b = (value >> (i * 7)) & 0x7F;
		if (i > 0) {
			b |= 0x80;
		}
		WriteUInt8(b);
	} while (i > 0);
}

MP4Property* MP4File::FindProperty(char* name)
{
	return m_pRootAtom->FindProperty(name);
}

// FindTrackProperty
// FindSampleProperty

MP4Property* MP4File::FindIntegerProperty(char* name)
{
	MP4Property* pProperty = FindProperty(name);
	if (!pProperty) {
		throw new MP4Error("no such property", "MP4File::FindIntegerProperty");
	}
	switch (pProperty->GetType()) {
	case Integer8Property:
	case Integer16Property:
	case Integer32Property:
	case Integer64Property:
		break;
	default:
		throw new MP4Error("type mismatch", "MP4File::FindIntegerProperty");
	}
	return pProperty;
}

u_int64_t MP4File::GetIntegerProperty(char* name)
{
	MP4Property* pProperty = FindIntegerProperty(name);
	switch (pProperty->GetType()) {
	case Integer8Property:
		return ((MP4Integer8Property*)pProperty)->GetValue();
	case Integer16Property:
		return ((MP4Integer16Property*)pProperty)->GetValue();
	case Integer32Property:
		return ((MP4Integer32Property*)pProperty)->GetValue();
	case Integer64Property:
		return ((MP4Integer64Property*)pProperty)->GetValue();
	}
	ASSERT(FALSE);
}

void MP4File::SetIntegerProperty(char* name, u_int64_t value)
{
	MP4Property* pProperty = FindIntegerProperty(name);

	switch (pProperty->GetType()) {
	case Integer8Property:
		((MP4Integer8Property*)pProperty)->SetValue(value);
		break;
	case Integer16Property:
		((MP4Integer16Property*)pProperty)->SetValue(value);
		break;
	case Integer32Property:
		((MP4Integer32Property*)pProperty)->SetValue(value);
		break;
	case Integer64Property:
		((MP4Integer64Property*)pProperty)->SetValue(value);
		break;
	default:
		ASSERT(FALSE);
	}
}

MP4StringProperty* MP4File::FindStringProperty(char* name)
{
	MP4Property* pProperty = FindProperty(name);
	if (!pProperty) {
		throw new MP4Error("no such property", "MP4File::FindStringProperty");
	}
	if (pProperty->GetType() != StringProperty) {
		throw new MP4Error("type mismatch", "MP4File::FindStringProperty");
	}
	return (MP4StringProperty*)pProperty;
}

const char* MP4File::GetStringProperty(char* name)
{
	MP4StringProperty* pProperty = FindStringProperty(name);
	return pProperty->GetValue();
}

void MP4File::SetStringProperty(char* name, char* value)
{
	MP4StringProperty* pProperty = FindStringProperty(name);
	pProperty->SetValue(value);
}

MP4BytesProperty* MP4File::FindBytesProperty(char* name)
{
	MP4Property* pProperty = FindProperty(name);
	if (!pProperty) {
		throw new MP4Error("no such property", "MP4File::FindBytesProperty");
	}
	if (pProperty->GetType() != BytesProperty) {
		throw new MP4Error("type mismatch", "MP4File::FindBytesProperty");
	}
	return (MP4BytesProperty*)pProperty;
}

void MP4File::GetBytesProperty(char* name, 
	u_int8_t** ppValue, u_int32_t* pValueSize)
{
	MP4BytesProperty* pProperty = FindBytesProperty(name);
	pProperty->GetValue(ppValue, pValueSize);
}

void MP4File::SetBytesProperty(char* name, u_int8_t* pValue, u_int32_t size)
{
	MP4BytesProperty* pProperty = FindBytesProperty(name);
	pProperty->SetValue(pValue, size);
}