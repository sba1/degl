/**
 * @file
 */

#include "core.hpp"

#include <stdio.h>
#include <string.h>

#include <clang-c/Index.h>

#include <algorithm>
#include <fstream>
#include <iosfwd>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct GlobalRef
{
	CXCursor referenceCursor;
	CXCursor globalCursor;
};

static std::vector<CXCursor> global_functions;
static std::map<std::string, CXCursor> global_var_map;
static std::vector<GlobalRef> refs_to_global_vars;
static std::string source;

static int offset(CXSourceLocation location)
{
	unsigned line, column, offset;
	CXFile file;
	clang_getFileLocation(location, &file, &line, &column, &offset);
	return offset;
}

static int offset(CXCursor cursor)
{
	return offset(clang_getCursorLocation(cursor));
}

static int offset(GlobalRef ref)
{
	return offset(ref.referenceCursor);
}

static enum CXChildVisitResult vistor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
	enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent);

	switch (cursor_kind)
	{
	case	CXCursor_FunctionDecl:
			global_functions.push_back(cursor);
			break;

	case	CXCursor_DeclRefExpr:
			{
				CXCursor refCursor = clang_getCursorReferenced(cursor);
				CXCursor parentRefCursor = clang_getCursorSemanticParent(refCursor);
				if (clang_getCursorKind(refCursor) == CXCursor_VarDecl && clang_getCursorKind(parentRefCursor) == CXCursor_TranslationUnit)
				{
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
	return CXChildVisit_Recurse;
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

	cerr << "Number of global variables: " << global_var_map.size() << endl;
	cerr << "Number of references: " << refs_to_global_vars.size() << endl;

	/* Sort decreasing in decreasing order */
	sort(refs_to_global_vars.begin(), refs_to_global_vars.end(),  [](GlobalRef a, GlobalRef b)
			{
				return offset(a) > offset(b);
			});

	for (auto ref : global_functions)
	{
//		CXSourceLocation location = clang_getCursorLocation(ref);
//		unsigned line, column, offset;
//		CXFile file;
//		clang_getFileLocation(location, &file, &line, &column, &offset);
//
		CXSourceRange range = clang_getCursorExtent(ref);
		int start = offset(clang_getRangeStart(range));
		int end = offset(clang_getRangeEnd(range));

		cout << start << "  " << end << endl;
	}

	for (auto ref : refs_to_global_vars)
	{
//		CXSourceLocation location = clang_getCursorLocation(ref.referenceCursor);
//		unsigned line, column, offset;
//		CXFile file;
//		clang_getFileLocation(location, &file, &line, &column, &offset);
//
		CXSourceRange range = clang_getCursorExtent(ref.referenceCursor);
		int start = offset(clang_getRangeStart(range));
		int end = offset(clang_getRangeEnd(range));

		/* Skip, if this is not the source file */
//		if (strcmp(filename, clang_getCString(clang_getFileName(file))))
//			continue;

		stringstream new_text;
		new_text << "context->" << clang_getCString(clang_getCursorDisplayName(ref.globalCursor));
		source.replace(start, end - start, new_text.str());
	}

	clang_disposeIndex(idx);

	cout << source << endl;
}
