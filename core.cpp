/**
 * @file
 */

#include "core.hpp"

#include <assert.h>
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

struct GlobalVariable
{
	CXCursor decl;
	CXCursor assignment;

	GlobalVariable() : decl(clang_getNullCursor()), assignment(clang_getNullCursor())
	{
	}
};

/**
 * Maps names of global variables to their defining cursor.
 */
static std::map<std::string, GlobalVariable> global_var_map;

/**
 * Represents a reference to a global variable
 */
struct GlobalRef
{
	CXCursor referenceCursor;
	CXCursor globalCursor;
};
static std::vector<GlobalRef> refs_to_global_vars;

/**
 * Represents a simple text edit of some source.
 */
struct TextEdit
{
	const char *filename; /**< Name of the file that is affected by the edit */
	int start;		/**< Starting offset of the edit */
	int length;		/**< Length of the text to be replaced */
	std::string new_string; /**< The text that replaces the region of the source text */

	/**
	 * Create a text edit with the entire range.
	 */
	static TextEdit fromCXCursor(CXCursor cursor)
	{
		CXSourceLocation loc = clang_getCursorLocation(cursor);

		CXFile file;
		unsigned line;
		unsigned column;
		unsigned off;
		clang_getSpellingLocation(loc, &file, &line, &column, &off);

		CXSourceRange range = clang_getCursorExtent(cursor);
		int start = offset(clang_getRangeStart(range));
		int end = offset(clang_getRangeEnd(range));

		TextEdit te;
		te.filename = clang_getCString(clang_getFileName(file));
		te.start = start;
		te.length = end - start;
		return te;
	}
};

/**
 * Represents a physical file and its contents.
 */
struct FilenameWithContents
{
	const char *filename;
	string source;

	FilenameWithContents(const char *filename_) : filename(filename_)
	{
		/* Load complete file as string */
		std::ifstream ifs(filename);
		source.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
	}
};

/****************************************************************/

static enum CXChildVisitResult vistor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
	static struct GlobalFunction *currentFunction = NULL;
	static struct GlobalVariable *currentVariable = NULL;

	enum CXCursorKind cursor_kind = clang_getCursorKind(cursor);
	enum CXCursorKind parent_cursor_kind = clang_getCursorKind(parent);

	if (currentVariable && clang_equalCursors(parent, currentVariable->decl))
		currentVariable->assignment = cursor;

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
			{
				GlobalVariable &glVariable = global_var_map[clang_getCString(clang_getCursorDisplayName(cursor))];
				glVariable.decl = cursor;
				currentVariable = &glVariable;
			}
			break;

	default:
			break;
	}
	return CXChildVisit_Recurse;
}

static void process_single_source_file(const char *filename)
{
}

void transform(std::vector<const char *> &filenames)
{
	assert(filenames.size() > 0);

	FilenameWithContents file(filenames[0]);

	/* Our lists of text edits */
	std::vector<TextEdit> text_edits;

	CXIndex idx = clang_createIndex(1, 1);
	CXTranslationUnit trunit = clang_createTranslationUnitFromSourceFile(idx, file.filename, 0, NULL, 0, NULL);
	CXCursor cursor = clang_getTranslationUnitCursor(trunit);

	/* Determine global variables and their references */
	clang_visitChildren(cursor, vistor, NULL);

	cerr << "Number of global functions: " << global_functions.size() << endl;
	cerr << "Number of global variables: " << global_var_map.size() << endl;
	cerr << "Number of references: " << refs_to_global_vars.size() << endl;

	/* Add to each collected global function the context parameter */
	for (auto ref : global_functions)
	{
		CXSourceRange range = clang_getCursorExtent(ref.decl);

		if (!clang_Cursor_isNull(ref.paramDecl))
		{
			TextEdit te = TextEdit::fromCXCursor(ref.paramDecl);

			stringstream new_text;
			new_text << "struct __context__ *__context__, ";
			new_text << file.source.substr(te.start, te.length);

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

	/* Replace each access to a global variable by an access to the context */
	for (auto ref : refs_to_global_vars)
	{
		stringstream new_text;
		new_text << "__context__->" << clang_getCString(clang_getCursorDisplayName(ref.globalCursor));

		TextEdit te = TextEdit::fromCXCursor(ref.referenceCursor);
		te.new_string = new_text.str();
		text_edits.push_back(te);
	}

	/* Sort text edits in decreasing order */
	sort(text_edits.begin(), text_edits.end(),  [](TextEdit a, TextEdit b)
			{
				return a.start > b.start;
			});

	cout << "struct __context__" << endl;
	cout << "{" << endl;

	for (auto it : global_var_map)
	{
		CXType type = clang_getCursorType(it.second.decl);
		CXString typeSpelling = clang_getTypeSpelling(type);
		CXString cursorSpelling = clang_getCursorSpelling(it.second.decl);

		cout << "    " << clang_getCString(typeSpelling) << " " << it.first << ";" << endl;

		/* Remove the global variables */
		TextEdit te = TextEdit::fromCXCursor(it.second.decl);
		text_edits.push_back(te);
	}
	cout << "};" << endl;

	std::string new_source = file.source;
	/* Now perform edit operations */
	for (auto te : text_edits)
	{
		if (strcmp(te.filename, file.filename))
			continue;
		new_source.replace(te.start, te.length, te.new_string);
	}

	cout << new_source << endl;

	/* Now write out the init function */
	cout << "void __init__context__(struct __context__ *__context__)" << endl;
	cout << "{" << endl;

	for (auto it : global_var_map)
	{
		if (clang_Cursor_isNull(it.second.assignment))
		{
			cout << "    __context__->" << it.first << " = 0;" << endl;
		} else
		{
			TextEdit te = TextEdit::fromCXCursor(it.second.assignment);
			cout << "    __context__->" << it.first << " = " << file.source.substr(te.start, te.length) << ";" << endl;
		}
	}

	cout << "}" << endl;

	clang_disposeIndex(idx);

}

void transform(const char *filename)
{
	std::vector<const char *> filenames;
	filenames.push_back(filename);

	transform(filenames);
}
