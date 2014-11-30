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
#include <unordered_map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

/****************************************************************/

void classify_args(vector<const char *> &filenames, vector<const char *> &options, int argc, const char **argv)
{
	bool compiler_options = false;
	for (int i=0; i < argc; i++)
	{
		if (!strcmp("--", argv[i])) compiler_options = true;
		else if (!compiler_options)
		{
			filenames.push_back(argv[i]);
		} else
		{
			options.push_back(argv[i]);
		}
	}
}

/****************************************************************/

static int offset(CXSourceLocation location)
{
	unsigned line, column, offset;
	CXFile file;
	clang_getFileLocation(location, &file, &line, &column, &offset);
	return offset;
}

__attribute__((unused))
static int offset(CXCursor cursor)
{
	return offset(clang_getCursorLocation(cursor));
}

static const char *filename(CXCursor cursor)
{
	CXFile file;
	unsigned line;
	unsigned column;
	unsigned off;

	CXSourceLocation loc = clang_getCursorLocation(cursor);
	clang_getSpellingLocation(loc, &file, &line, &column, &off);

	return clang_getCString(clang_getFileName(file));
}

__attribute__((unused))
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
static vector<GlobalFunction> global_functions;

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
static map<string, GlobalVariable> global_var_map;

/**
 * All function calls.
 */
static vector<CXCursor> function_calls;

/**
 * Represents a reference to a global variable
 */
struct GlobalRef
{
	CXCursor referenceCursor;
	CXCursor globalCursor;
};
static vector<GlobalRef> refs_to_global_vars;

/**
 * Represents a simple text edit of some source.
 */
struct TextEdit
{
	const char *filename; /**< Name of the file that is affected by the edit */
	int start;		/**< Starting offset of the edit */
	int length;		/**< Length of the text to be replaced */
	string new_string; /**< The text that replaces the region of the source text */

	/**
	 * Create a text edit with the entire range.
	 */
	static TextEdit fromCXCursor(CXCursor cursor)
	{
		CXSourceRange range = clang_getCursorExtent(cursor);
		int start = offset(clang_getRangeStart(range));
		int end = offset(clang_getRangeEnd(range));

		TextEdit te;
		te.filename = ::filename(cursor);
		te.start = start;
		te.length = end - start;
		return te;
	}

private:
	TextEdit()
	{
	}
};

ostream &operator<<(ostream &os, const TextEdit &te)
{
	os << "(" << te.filename << ", " << te.start << ", " << te.length << ")";
	return os;
}

/**
 * Represents a physical file and its contents.
 */
struct File
{
	const char *filename;
	string source;

	File(const char *filename_) : filename(filename_)
	{
		/* Load complete file as string */
		ifstream ifs(filename);
		source.assign(istreambuf_iterator<char>(ifs), istreambuf_iterator<char>());
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

	case	CXCursor_CallExpr:
			function_calls.push_back(cursor);
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
				CXString display_name = clang_getCursorDisplayName(cursor);
				GlobalVariable &glVariable = global_var_map[clang_getCString(display_name)]; /* string will be copied */
				clang_disposeString(display_name);
				glVariable.decl = cursor;
				currentVariable = &glVariable;
			}
			break;

	default:
			break;
	}
	return CXChildVisit_Recurse;
}

/**
 * A map containing all files that are considered.
 */
static unordered_map<string, File> fileMap;

/**
 * Insert a file into our file map.
 *
 * @param filename
 */
static void insert_file(const char *filename)
{
	fileMap.insert(make_pair(filename, File(filename)));
}

/**
 * Returns the file given by the filename. If the file is not loaded,
 * it will be loaded.
 *
 * @param filename
 * @return a ref to the requested file.
 */
static File &get_file(const char *filename)
{
	auto file = fileMap.find(filename);
	if (file == fileMap.end())
	{
		insert_file(filename);
		file = fileMap.find(filename);
	}
	return file->second;
}

/**
 * Process a single source file.
 *
 * @param filename
 */
static CXTranslationUnit process_single_source_file(const char *filename, const vector<const char *> &args, CXIndex idx)
{
	CXTranslationUnit trunit = clang_createTranslationUnitFromSourceFile(idx, filename, args.size(), &args[0], 0, NULL);
	CXCursor cursor = clang_getTranslationUnitCursor(trunit);

	/* Determine global variables and their references */
	clang_visitChildren(cursor, vistor, NULL);

	return trunit;
}

void transform(vector<const char *> &filenames, vector <const char *> &options)
{
	assert(filenames.size() > 0);

	/* List of translation units */
	vector<CXTranslationUnit> tr_units;

	/* Our lists of text edits */
	vector<TextEdit> text_edits;

	CXIndex idx = clang_createIndex(1, 1);

	for (auto filename : filenames)
	{
		insert_file(filename);
		tr_units.push_back(process_single_source_file(filename, vector<const char*>(), idx));
	}

	cerr << "Number of global functions: " << global_functions.size() << endl;
	cerr << "Number of global variables: " << global_var_map.size() << endl;
	cerr << "Number of references: " << refs_to_global_vars.size() << endl;

	/* Add to each collected global function the context parameter */
	for (auto ref : global_functions)
	{
		if (!clang_Cursor_isNull(ref.paramDecl))
		{
			TextEdit te = TextEdit::fromCXCursor(ref.paramDecl);

			stringstream new_text;
			new_text << "struct __context__ *__context__, ";
			new_text << get_file(te.filename).source.substr(te.start, te.length);

			te.new_string = new_text.str();
			text_edits.push_back(te);
		} else
		{
			TextEdit te = TextEdit::fromCXCursor(ref.decl);
			if (!clang_Cursor_isNull(ref.block))
			{
				TextEdit te2 = TextEdit::fromCXCursor(ref.block);
				te.length = te2.start - te.start;
			}
			stringstream new_text;

			CXType type = clang_getCursorResultType(ref.decl);
			CXString typeSpelling = clang_getTypeSpelling(type);
			CXString cursorSpelling = clang_getCursorSpelling(ref.decl);

			new_text << clang_getCString(typeSpelling) << " " << clang_getCString(cursorSpelling);
			new_text << "(struct __context__ *__context__)" << endl;

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

	/* Insert to each call of the function the context parameter */
	for (auto callCursor : function_calls)
	{
		TextEdit te = TextEdit::fromCXCursor(callCursor);
		te.new_string = get_file(te.filename).source.substr(te.start, te.length);
		int pos = te.new_string.find('(');
		int numArgs = clang_Cursor_getNumArguments(callCursor);
		te.new_string.insert(pos + 1, string("__context__").append(numArgs==0?"":","));
		text_edits.push_back(te);
	}

	stringstream context_header;
	stringstream context_source;

	context_header << "struct __context__" << endl;
	context_header << "{" << endl;

	for (auto it : global_var_map)
	{
		CXType type = clang_getCursorType(it.second.decl);
		CXString typeSpelling = clang_getTypeSpelling(type);

		context_header << "    " << clang_getCString(typeSpelling) << " " << it.first << ";" << endl;

		/* Remove the global variables */
		TextEdit te = TextEdit::fromCXCursor(it.second.decl);
		text_edits.push_back(te);
	}
	context_header << "};" << endl;

	/* Output the context source file */
	context_source << "void __init__context__(struct __context__ *__context__)" << endl;
	context_source << "{" << endl;

	for (auto it : global_var_map)
	{
		if (clang_Cursor_isNull(it.second.assignment))
		{
			context_source << "    __context__->" << it.first << " = 0;" << endl;
		} else
		{
			TextEdit te = TextEdit::fromCXCursor(it.second.assignment);
			context_source << "    __context__->" << it.first << " = " << get_file(te.filename).source.substr(te.start, te.length) << ";" << endl;
		}
	}

	context_source << "}" << endl;


	/* Sort text edits in decreasing order */
	sort(text_edits.begin(), text_edits.end(),  [](const TextEdit &a, const TextEdit &b)
			{
				int comp = strcmp(a.filename, b.filename);
				if (comp > 0) return true;
				if (comp < 0) return false;

				return a.start > b.start;
			});

	/* Filter duplicates */
	auto end = unique(text_edits.begin(), text_edits.end(), [](const TextEdit &a, const TextEdit &b)
		{
			if (strcmp(a.filename, b.filename))
				return false;
			return (a.start == b.start) && (a.length == b.length);
		});
	text_edits.erase(end, text_edits.end());

	/* Insert text edit eol marker */
	const char *prev_filename = "";
	string new_source;

	/* Now perform edit operations */
	for (auto te : text_edits)
	{
		if (strcmp(te.filename, prev_filename))
		{
			/* Output previous file */
			if (strlen(prev_filename))
			{
				cout << "/* " << prev_filename << " */" << endl;
				cout << new_source << endl;
			}

			new_source = get_file(te.filename).source;
			prev_filename = te.filename;
		}
		new_source.replace(te.start, te.length, te.new_string);
	}

	/* Output final file */
	cout << "/* " << prev_filename << " */" << endl;
	cout << new_source << endl;

	/* Output the context header file */
	cout << "/* __context__.h */" << endl;
	cout << context_header.str() << endl;

	/* Output the context source file */
	cout << "/* __context__.c */" << endl;
	cout << context_source.str() << endl;


	for (auto &tr_unit : tr_units)
		clang_disposeTranslationUnit(tr_unit);

	clang_disposeIndex(idx);

}
