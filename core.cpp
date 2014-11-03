/**
 * @file
 */

#include "core.hpp"

#include <clang-c/Index.h>
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <vector>

//std::vector<CXCursor> functionDecls;
struct GlobalRef
{
	CXCursor referenceCursor;
	CXCursor globalCursor;
};

static std::map<std::string, CXCursor> global_var_map;
static std::vector<GlobalRef> refs_to_global_vars;
static std::string source;

static enum CXChildVisitResult vistor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
	enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent);

//	CXSourceRange source_range = clang_getCursorExtent(cursor);
//	printf("%d (%d,%d) %s\n", cursor_kind, source_range.begin_int_data, source_range.end_int_data,  clang_getCString(clang_getCursorDisplayName(cursor)));
//	printf("%d\n", clang_getCString(clang_getCursorDisplayName(clang_getCursorSemanticParent(cursor))));

	switch (cursor_kind)
	{
	case	CXCursor_FunctionDecl:
			printf("FunctionDeclaration: %s\n", clang_getCString(clang_getCursorDisplayName(cursor)));
			break;

	case	CXCursor_DeclRefExpr:
			{
				CXCursor refCursor = clang_getCursorReferenced(cursor);
				CXCursor parentRefCursor = clang_getCursorSemanticParent(refCursor);
				if (clang_getCursorKind(refCursor) == CXCursor_VarDecl && clang_getCursorKind(parentRefCursor) == CXCursor_TranslationUnit)
				{
					printf("Access to global variable: %s\n", clang_getCString(clang_getCursorDisplayName(cursor)));
					GlobalRef global_ref = {cursor, refCursor};
					refs_to_global_vars.push_back(global_ref);
				}
			}
			break;

	case	CXCursor_VarDecl:
			/* Remember all global variables */
			if (parent_cursor_kind == CXCursor_TranslationUnit)
				global_var_map[clang_getCString(clang_getCursorDisplayName(cursor))] = cursor;
			break;

	default:
			break;
	}
	return CXChildVisit_Recurse;//CXChildVisit_Continue;
}

void transform(const char *filename)
{
	/* Load complete file as string */
	std::ifstream ifs(filename);
	source.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

	CXIndex idx = clang_createIndex(1, 1);
	CXTranslationUnit trunit = clang_createTranslationUnitFromSourceFile(idx, filename, 0, NULL, 0, NULL);
	CXCursor cursor = clang_getTranslationUnitCursor(trunit);

//	CXSourceRange source_range = clang_getCursorExtent(cursor);
//	printf("(%d,%d)\n", clang_getRangeStart(source_range), clang_getRangeEnd(source_range));
//
//	printf("sss %d\n", clang_getCursorKind(cursor));

	/* Determine global variables and their references */
	clang_visitChildren(cursor, vistor, NULL);

//	clang_saveTranslationUnit(trunit, "/tmp/test.c", 0);

	std::cout << "Number of global variables: " << global_var_map.size() << std::endl;
	std::cout << "Number of references: " << refs_to_global_vars.size() << std::endl;

	for (auto ref  : refs_to_global_vars)
	{
		CXSourceLocation location = clang_getCursorLocation(ref.globalCursor);
		unsigned line, column, offset;
		CXFile file;

		clang_getFileLocation(location, &file, &line, &column, &offset);

		std::cout << clang_getCString(clang_getFileName(file)) << std::endl;
	}

	clang_disposeIndex(idx);
}
