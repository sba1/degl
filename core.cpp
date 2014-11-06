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


/****************************************************************/

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

static void print_range(CXCursor cursor)
{
	CXSourceRange range = clang_getCursorExtent(cursor);
	int start = offset(clang_getRangeStart(range));
	int end = offset(clang_getRangeEnd(range));

	cout << "(" << start << ", " << end <<")" << endl;
}

/**
 * Represents a global function with parameter and statement block.
 */
struct GlobalFunction
{
	CXCursor decl;
	CXCursor paramDecl;
	CXCursor block;

	GlobalFunction() : decl(clang_getNullCursor()), paramDecl(clang_getNullCursor()), block(clang_getNullCursor())
	{
	}
};

/**
 * The list of global functions that we encounter
 */
static std::vector<GlobalFunction> global_functions;

static std::map<std::string, CXCursor> global_var_map;

struct GlobalRef
{
	CXCursor referenceCursor;
	CXCursor globalCursor;
};
static std::vector<GlobalRef> refs_to_global_vars;

struct TextEdit
{
	int start;
	int length;
	std::string new_string;

	static TextEdit fromCXCursor(CXCursor cursor)
	{
		TextEdit te;

		CXSourceRange range = clang_getCursorExtent(cursor);
		int start = offset(clang_getRangeStart(range));
		int end = offset(clang_getRangeEnd(range));
		te.start = start;
		te.length = end - start;
		return te;
	}
};

static std::string source;

/****************************************************************/

static enum CXChildVisitResult vistor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
	static struct GlobalFunction *currentFunction = NULL;

	enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent);

	switch (cursor_kind)
	{
	case	CXCursor_FunctionDecl:
			{
				GlobalFunction function;
				function.decl = cursor;
				global_functions.push_back(function);
				currentFunction = &global_functions[global_functions.size() - 1];
			}
			break;

	case	CXCursor_ParmDecl:
			if (currentFunction && clang_equalCursors(parent, currentFunction->decl))
				currentFunction->paramDecl = cursor;
			break;

	case	CXCursor_CompoundStmt:
			if (currentFunction && clang_equalCursors(parent, currentFunction->decl))
				currentFunction->block = cursor;
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

	std::vector<TextEdit> text_edits;

	CXIndex idx = clang_createIndex(1, 1);
	CXTranslationUnit trunit = clang_createTranslationUnitFromSourceFile(idx, filename, 0, NULL, 0, NULL);
	CXCursor cursor = clang_getTranslationUnitCursor(trunit);

	/* Determine global variables and their references */
	clang_visitChildren(cursor, vistor, NULL);

	cerr << "Number of global variables: " << global_var_map.size() << endl;
	cerr << "Number of references: " << refs_to_global_vars.size() << endl;

	for (auto ref : global_functions)
	{
		CXSourceRange range = clang_getCursorExtent(ref.decl);

		if (!clang_Cursor_isNull(ref.paramDecl))
		{
			TextEdit te = TextEdit::fromCXCursor(ref.paramDecl);

			stringstream new_text;
			new_text << "struct __context__ *__context__, ";
			new_text << source.substr(te.start, te.length);

			te.new_string = new_text.str();
			text_edits.push_back(te);
		} else
		{
			TextEdit te = TextEdit::fromCXCursor(ref.decl);
			TextEdit te2 = TextEdit::fromCXCursor(ref.block);
			te.length = te2.start - te.start;
			stringstream new_text;

			CXType type = clang_getCursorResultType(ref.decl);
			CXString typeSpelling = clang_getTypeSpelling(type);
			CXString cursorSpelling = clang_getCursorSpelling(ref.decl);

			new_text << clang_getCString(typeSpelling) << " " << clang_getCString(cursorSpelling);
			new_text << "(struct context __context__ *__context__)" << endl;

			te.new_string = new_text.str();
			text_edits.push_back(te);
		}
	}

	for (auto ref : refs_to_global_vars)
	{
		stringstream new_text;
		new_text << "__context__->" << clang_getCString(clang_getCursorDisplayName(ref.globalCursor));

		TextEdit te = TextEdit::fromCXCursor(ref.referenceCursor);
		te.new_string = new_text.str();
		text_edits.push_back(te);
	}

	/* Sort decreasing in decreasing order */
	sort(text_edits.begin(), text_edits.end(),  [](TextEdit a, TextEdit b)
			{
				return a.start > b.start;
			});

	cout << "struct __context__" << endl;
	cout << "{" << endl;

	for (auto it : global_var_map)
	{
		CXType type = clang_getCursorType(it.second);
		CXString typeSpelling = clang_getTypeSpelling(type);
		CXString cursorSpelling = clang_getCursorSpelling(it.second);

		cout << "    " << clang_getCString(typeSpelling) << " " << it.first << ";" << endl;
	}
	cout << "}" << endl;

	/* Now perform edit operations */
	for (auto te : text_edits)
		source.replace(te.start, te.length, te.new_string);

	clang_disposeIndex(idx);

	cout << source << endl;
}
