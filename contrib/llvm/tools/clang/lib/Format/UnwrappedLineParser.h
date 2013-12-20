//===--- UnwrappedLineParser.h - Format C++ code ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file contains the declaration of the UnwrappedLineParser,
/// which turns a stream of tokens into UnwrappedLines.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FORMAT_UNWRAPPED_LINE_PARSER_H
#define LLVM_CLANG_FORMAT_UNWRAPPED_LINE_PARSER_H

#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Format/Format.h"
#include "clang/Lex/Lexer.h"
#include <list>

namespace clang {

class DiagnosticsEngine;

namespace format {

/// \brief A wrapper around a \c Token storing information about the
/// whitespace characters preceeding it.
struct FormatToken {
  FormatToken()
      : NewlinesBefore(0), HasUnescapedNewline(false), WhiteSpaceLength(0),
        LastNewlineOffset(0), TokenLength(0), IsFirst(false),
        MustBreakBefore(false), TrailingWhiteSpaceLength(0) {}

  /// \brief The \c Token.
  Token Tok;

  /// \brief The number of newlines immediately before the \c Token.
  ///
  /// This can be used to determine what the user wrote in the original code
  /// and thereby e.g. leave an empty line between two function definitions.
  unsigned NewlinesBefore;

  /// \brief Whether there is at least one unescaped newline before the \c
  /// Token.
  bool HasUnescapedNewline;

  /// \brief The location of the start of the whitespace immediately preceeding
  /// the \c Token.
  ///
  /// Used together with \c WhiteSpaceLength to create a \c Replacement.
  SourceLocation WhiteSpaceStart;

  /// \brief The length in characters of the whitespace immediately preceeding
  /// the \c Token.
  unsigned WhiteSpaceLength;

  /// \brief The offset just past the last '\n' in this token's leading
  /// whitespace (relative to \c WhiteSpaceStart). 0 if there is no '\n'.
  unsigned LastNewlineOffset;

  /// \brief The length of the non-whitespace parts of the token. This is
  /// necessary because we need to handle escaped newlines that are stored
  /// with the token.
  unsigned TokenLength;

  /// \brief Indicates that this is the first token.
  bool IsFirst;

  /// \brief Whether there must be a line break before this token.
  ///
  /// This happens for example when a preprocessor directive ended directly
  /// before the token.
  bool MustBreakBefore;

  /// \brief Number of characters of trailing whitespace.
  unsigned TrailingWhiteSpaceLength;

  /// \brief Returns actual token start location without leading escaped
  /// newlines and whitespace.
  ///
  /// This can be different to Tok.getLocation(), which includes leading escaped
  /// newlines.
  SourceLocation getStartOfNonWhitespace() const {
    return WhiteSpaceStart.getLocWithOffset(WhiteSpaceLength);
  }
};

/// \brief An unwrapped line is a sequence of \c Token, that we would like to
/// put on a single line if there was no column limit.
///
/// This is used as a main interface between the \c UnwrappedLineParser and the
/// \c UnwrappedLineFormatter. The key property is that changing the formatting
/// within an unwrapped line does not affect any other unwrapped lines.
struct UnwrappedLine {
  UnwrappedLine() : Level(0), InPPDirective(false), MustBeDeclaration(false) {
  }

  // FIXME: Don't use std::list here.
  /// \brief The \c Tokens comprising this \c UnwrappedLine.
  std::list<FormatToken> Tokens;

  /// \brief The indent level of the \c UnwrappedLine.
  unsigned Level;

  /// \brief Whether this \c UnwrappedLine is part of a preprocessor directive.
  bool InPPDirective;

  bool MustBeDeclaration;
};

class UnwrappedLineConsumer {
public:
  virtual ~UnwrappedLineConsumer() {
  }
  virtual void consumeUnwrappedLine(const UnwrappedLine &Line) = 0;
};

class FormatTokenSource {
public:
  virtual ~FormatTokenSource() {
  }
  virtual FormatToken getNextToken() = 0;
};

class UnwrappedLineParser {
public:
  UnwrappedLineParser(clang::DiagnosticsEngine &Diag, const FormatStyle &Style,
                      FormatTokenSource &Tokens,
                      UnwrappedLineConsumer &Callback);

  /// Returns true in case of a structural error.
  bool parse();

private:
  void parseFile();
  void parseLevel(bool HasOpeningBrace);
  void parseBlock(bool MustBeDeclaration, unsigned AddLevels = 1);
  void parsePPDirective();
  void parsePPDefine();
  void parsePPUnknown();
  void parseStructuralElement();
  void parseBracedList();
  void parseReturn();
  void parseParens();
  void parseIfThenElse();
  void parseForOrWhileLoop();
  void parseDoWhile();
  void parseLabel();
  void parseCaseLabel();
  void parseSwitch();
  void parseNamespace();
  void parseAccessSpecifier();
  void parseEnum();
  void parseRecord();
  void parseObjCProtocolList();
  void parseObjCUntilAtEnd();
  void parseObjCInterfaceOrImplementation();
  void parseObjCProtocol();
  void addUnwrappedLine();
  bool eof() const;
  void nextToken();
  void readToken();
  void flushComments(bool NewlineBeforeNext);
  void pushToken(const FormatToken &Tok);

  // FIXME: We are constantly running into bugs where Line.Level is incorrectly
  // subtracted from beyond 0. Introduce a method to subtract from Line.Level
  // and use that everywhere in the Parser.
  OwningPtr<UnwrappedLine> Line;

  // Comments are sorted into unwrapped lines by whether they are in the same
  // line as the previous token, or not. If not, they belong to the next token.
  // Since the next token might already be in a new unwrapped line, we need to
  // store the comments belonging to that token.
  SmallVector<FormatToken, 1> CommentsBeforeNextToken;
  FormatToken FormatTok;
  bool MustBreakBeforeNextToken;

  // The parsed lines. Only added to through \c CurrentLines.
  std::vector<UnwrappedLine> Lines;

  // Preprocessor directives are parsed out-of-order from other unwrapped lines.
  // Thus, we need to keep a list of preprocessor directives to be reported
  // after an unwarpped line that has been started was finished.
  std::vector<UnwrappedLine> PreprocessorDirectives;

  // New unwrapped lines are added via CurrentLines.
  // Usually points to \c &Lines. While parsing a preprocessor directive when
  // there is an unfinished previous unwrapped line, will point to
  // \c &PreprocessorDirectives.
  std::vector<UnwrappedLine> *CurrentLines;

  // We store for each line whether it must be a declaration depending on
  // whether we are in a compound statement or not.
  std::vector<bool> DeclarationScopeStack;

  // Will be true if we encounter an error that leads to possibily incorrect
  // indentation levels.
  bool StructuralError;

  clang::DiagnosticsEngine &Diag;
  const FormatStyle &Style;
  FormatTokenSource *Tokens;
  UnwrappedLineConsumer &Callback;

  friend class ScopedLineState;
};

} // end namespace format
} // end namespace clang

#endif // LLVM_CLANG_FORMAT_UNWRAPPED_LINE_PARSER_H
