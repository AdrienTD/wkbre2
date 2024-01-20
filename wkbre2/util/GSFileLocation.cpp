#include "GSFileLocation.h"
#include "IndexedStringList.h"
#include "GSFileParser.h"

static IndexedStringList g_GSFileLocation_registeredFiles;

GSFileLocation::FileIndex GSFileLocation::registerFile(const std::string& filePath)
{
	return FileIndex(g_GSFileLocation_registeredFiles.insertString(filePath));
}

void GSFileLocation::set(GSFileParser& gsf)
{
	if (gsf.fileIndex == FileIndex(-1))
		gsf.fileIndex = registerFile(gsf.fileName);
	_fileIndex = FileIndex(gsf.fileIndex);
	_lineIndex = LineIndex(gsf.linenum);
}

std::string GSFileLocation::asString() const
{
	auto& fileName = g_GSFileLocation_registeredFiles.getString(_fileIndex);
	return fileName + " (line " + std::to_string(_lineIndex) + ')';
}
