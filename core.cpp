/**
 * @file
 */

#include "core.hpp"

#include <clang-c/Index.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <map>

//std::vector<CXCursor> functionDecls;
static std::map<std::string, CXCursor> global_var_map;

static enum CXChildVisitResult vistor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
	enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent);

//	CXSourceRange source_range = clang_getCursorExtent(cursor);
//	printf("(%d,%d)\n", source_range.begin_int_data, source_range.end_int_data);

	if (cursor_kind)
	switch (cursor_kind)
	{
	case	CXCursor_FunctionDecl:
			printf("%s\n", clang_getCString(clang_getCursorDisplayName(cursor)));
			printf("visit: %d\n", cursor_kind);
			break;

	case	CXCursor_VarDecl:
			/* Remember all global variables */
			if (parent_cursor_kind == CXCursor_TranslationUnit)
			{
//				printf("%s %d %d\n", clang_getCString(clang_getCursorDisplayName(cursor)), parent_cursor_kind, clang_getCursorKind(clang_getCursorSemanticParent(cursor)));
				global_var_map[clang_getCString(clang_getCursorDisplayName(cursor))] = cursor;
			}

			break;
	default:
			break;
	}
	return CXChildVisit_Recurse;//CXChildVisit_Continue;
}

void transform(const char *filename)
{
	CXIndex idx = clang_createIndex(1, 1);

	CXTranslationUnit trunit = clang_createTranslationUnitFromSourceFile(idx, filename, 0, NULL, 0, NULL);

	CXCursor cursor = clang_getTranslationUnitCursor(trunit);

//	CXSourceRange source_range = clang_getCursorExtent(cursor);
//	printf("(%d,%d)\n", clang_getRangeStart(source_range), clang_getRangeEnd(source_range));
//
//	printf("sss %d\n", clang_getCursorKind(cursor));

	clang_visitChildren(cursor, vistor, NULL);

//	clang_saveTranslationUnit(trunit, "/tmp/test.c", 0);

	std::cout << "Number of global variables: %d\n" << global_var_map.size() << std::endl;

	clang_disposeIndex(idx);
}
