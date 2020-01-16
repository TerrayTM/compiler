//--------------------------------------------------------------------------------------
//| Scanner to recognize tokens of the language using simplified maximum munch and DFA |
//--------------------------------------------------------------------------------------

#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

using namespace std;

enum class State {
  INIT,
  ID,
  NUM,
  END,
  EQ,
  NE,
  LT,
  GT,
  ERR,
  SPA,
  ZERO
};

enum class TokenKind {
  ID,
  NUM,
  LPAREN,
  RPAREN,
  LBRACE,
  RBRACE,
  RETURN,
  IF,
  ELSE,
  WHILE,
  PRINTLN,
  WAIN,
  BECOMES,
  INT,
  EQ,
  NE,
  LT,
  GT,
  LE,
  GE,
  PLUS,
  MINUS,
  STAR,
  SLASH,
  PCT,
  COMMA,
  SEMI,
  NEW,
  DELETE,
  LBRACK,
  RBRACK,
  AMP,
  TNULL,
  SPACE
};

unordered_map<TokenKind, string> kindToString = {
  {TokenKind::ID, "ID"},
  {TokenKind::NUM, "NUM"},
  {TokenKind::LPAREN, "LPAREN"},
  {TokenKind::RPAREN, "RPAREN"},
  {TokenKind::LBRACE, "LBRACE"},
  {TokenKind::RBRACE, "RBRACE"},
  {TokenKind::RETURN, "RETURN"},
  {TokenKind::IF, "IF"},
  {TokenKind::ELSE, "ELSE"},
  {TokenKind::WHILE, "WHILE"},
  {TokenKind::PRINTLN, "PRINTLN"},
  {TokenKind::WAIN, "WAIN"},
  {TokenKind::BECOMES, "BECOMES"},
  {TokenKind::INT, "INT"},
  {TokenKind::EQ, "EQ"},
  {TokenKind::NE, "NE"},
  {TokenKind::LT, "LT"},
  {TokenKind::GT, "GT"},
  {TokenKind::LE, "LE"},
  {TokenKind::GE, "GE"},
  {TokenKind::PLUS, "PLUS"},
  {TokenKind::MINUS, "MINUS"},
  {TokenKind::STAR, "STAR"},
  {TokenKind::SLASH, "SLASH"},
  {TokenKind::PCT, "PCT"},
  {TokenKind::COMMA, "COMMA"},
  {TokenKind::SEMI, "SEMI"},
  {TokenKind::NEW, "NEW"},
  {TokenKind::DELETE, "DELETE"},
  {TokenKind::LBRACK, "LBRACK"},
  {TokenKind::RBRACK, "RBRACK"},
  {TokenKind::AMP, "AMP"},
  {TokenKind::TNULL, "NULL"}
};

unordered_map<string, TokenKind> identifierMapping = {
  {"return", TokenKind::RETURN},
  {"if", TokenKind::IF},
  {"else", TokenKind::ELSE},
  {"while", TokenKind::WHILE},
  {"println", TokenKind::PRINTLN},
  {"wain", TokenKind::WAIN},
  {"int", TokenKind::INT},
  {"NULL", TokenKind::TNULL},
  {"new", TokenKind::NEW},
  {"delete", TokenKind::DELETE}
};

unordered_map<string, TokenKind> endMapping = {
  {"(", TokenKind::LPAREN},
  {")", TokenKind::RPAREN},
  {"{", TokenKind::LBRACE},
  {"}", TokenKind::RBRACE},
  {"==", TokenKind::EQ},
  {"!=", TokenKind::NE},
  {"<=", TokenKind::LE},
  {">=", TokenKind::GE},
  {"+", TokenKind::PLUS},
  {"-", TokenKind::MINUS},
  {"*", TokenKind::STAR},
  {"/", TokenKind::SLASH},
  {"%", TokenKind::PCT},
  {",", TokenKind::COMMA},
  {";", TokenKind::SEMI},
  {"[", TokenKind::LBRACK},
  {"]", TokenKind::RBRACK},
  {"&", TokenKind::AMP}
};

unordered_map<string, State> edges;

unordered_set<State> accepts = {
  State::ID,
  State::NUM,
  State::EQ,
  State::LT,
  State::GT,
  State::END,
  State::SPA,
  State::ZERO
};

struct Token {
  private:
    TokenKind kind;
    string lexeme;
  public:
    Token(TokenKind kind, string lexeme) : kind(kind), lexeme(lexeme) {}
    string toString() { return kindToString[kind] + " " + lexeme; }
    bool isEmpty() { return kind == TokenKind::SPACE; }
    TokenKind getKind() { return kind; }
};

inline string edgeHash(State state, char gate) {
  return to_string((int)state) + "#" + gate;
}

void registerStateEdge(State from, State to, char gate) {
  edges.insert({edgeHash(from, gate), to});
}

State moveState(State current, char character) {
  string key = edgeHash(current, character);

  if (edges.find(key) != edges.end()) {
    return edges[key];
  } else {
    return State::ERR;
  }
}

Token mapState(State state, string lexeme) {
  TokenKind kind;

  switch (state) {
    case State::ID:
      if (identifierMapping.find(lexeme) != identifierMapping.end()) {
        kind = identifierMapping[lexeme];
      } else {
        kind = TokenKind::ID;
      }
      break;

    case State::NUM:
      kind = TokenKind::NUM;
      break;

    case State::EQ:
      kind = TokenKind::BECOMES;
      break;

    case State::LT:
      kind = TokenKind::LT;
      break;
      
    case State::GT:
      kind = TokenKind::GT;
      break;

    case State::SPA:
      kind = TokenKind::SPACE;
      break;

    case State::ZERO:
      kind = TokenKind::NUM;
      break;

    case State::END:
      kind = endMapping[lexeme];
      break;
  }

  return Token(kind, lexeme);
}

bool validNumber(string lexeme) {
  if (lexeme.size() > 10) {
    return false;
  }

  long long candidate = stoll(lexeme);

  if (candidate > 2147483647) {
    return false;
  }

  return true;
}

unordered_set<TokenKind> whitespaceOne = {
  TokenKind::ID,
  TokenKind::NUM,
  TokenKind::RETURN,
  TokenKind::IF,
  TokenKind::ELSE,
  TokenKind::WHILE,
  TokenKind::PRINTLN,
  TokenKind::WAIN,
  TokenKind::INT,
  TokenKind::NEW,
  TokenKind::TNULL,
  TokenKind::DELETE
};

unordered_set<TokenKind> whitespaceTwo = {
  TokenKind::EQ,
  TokenKind::NE,
  TokenKind::LT,
  TokenKind::LE,
  TokenKind::GT,
  TokenKind::GE,
  TokenKind::BECOMES
};

bool ensureCorrectness(vector<Token>& tokens) {
  for (int i = 0; i + 1 < tokens.size(); ++i) {
    TokenKind kindOne = tokens.at(i).getKind();
    TokenKind kindTwo = tokens.at(i + 1).getKind();

    if(whitespaceOne.find(kindOne) != whitespaceOne.end() && whitespaceOne.find(kindTwo) != whitespaceOne.end()) {
      return false;
    } 

    if(whitespaceTwo.find(kindOne) != whitespaceTwo.end() && whitespaceTwo.find(kindTwo) != whitespaceTwo.end()) {
      return false;
    } 
  }

  return true;
}

bool scan(string code, vector<Token>& tokens) {
  int i = 0;
  string lexeme = "";
  State current = State::INIT;
  State next;

  while (true) {
    if (i < code.size()) {
      next = moveState(current, code[i]);
    } else {
      next = State::ERR;
    }

    if (next == State::ERR) {
      if (accepts.find(current) == accepts.end()) {
        return false;
      }

      if (current == State::NUM && !validNumber(lexeme)) {
        return false;
      }

      tokens.push_back(mapState(current, lexeme));

      current = State::INIT;
      lexeme = "";
      
      if (i == code.size()) {
        return true;
      }
    } else {
      lexeme += code[i];
      current = next;
      ++i;
    }
  }
}

int main() {
  for (int i = 0; i < 26; ++i) {
    registerStateEdge(State::INIT, State::ID, 'a' + i);
  }

  for (int i = 0; i < 26; ++i) {
    registerStateEdge(State::INIT, State::ID, 'A' + i);
  }

  for (int i = 0; i < 26; ++i) {
    registerStateEdge(State::ID, State::ID, 'a' + i);
  }

  for (int i = 0; i < 26; ++i) {
    registerStateEdge(State::ID, State::ID, 'A' + i);
  }

  for (int i = 0; i < 10; ++i) {
    registerStateEdge(State::ID, State::ID, '0' + i);
  }

  for (int i = 1; i < 10; ++i) {
    registerStateEdge(State::INIT, State::NUM, '0' + i);
  }

  for (int i = 0; i < 10; ++i) {
    registerStateEdge(State::NUM, State::NUM, '0' + i);
  }

  registerStateEdge(State::INIT, State::ZERO, '0');
  registerStateEdge(State::INIT, State::SPA, ' ');
  registerStateEdge(State::INIT, State::SPA, '\t');
  registerStateEdge(State::INIT, State::SPA, '\n');
  registerStateEdge(State::SPA, State::SPA, ' ');
  registerStateEdge(State::SPA, State::SPA, '\t');
  registerStateEdge(State::SPA, State::SPA, '\n');
  registerStateEdge(State::INIT, State::END, '(');
  registerStateEdge(State::INIT, State::END, ')');
  registerStateEdge(State::INIT, State::END, '{');
  registerStateEdge(State::INIT, State::END, '}');
  registerStateEdge(State::INIT, State::EQ, '=');
  registerStateEdge(State::EQ, State::END, '=');
  registerStateEdge(State::INIT, State::NE, '!');
  registerStateEdge(State::NE, State::END, '=');
  registerStateEdge(State::INIT, State::LT, '<');
  registerStateEdge(State::INIT, State::GT, '>');
  registerStateEdge(State::LT, State::END, '=');
  registerStateEdge(State::GT, State::END, '=');
  registerStateEdge(State::INIT, State::END, '+');
  registerStateEdge(State::INIT, State::END, '-');
  registerStateEdge(State::INIT, State::END, '*');
  registerStateEdge(State::INIT, State::END, '/');
  registerStateEdge(State::INIT, State::END, '%');
  registerStateEdge(State::INIT, State::END, ',');
  registerStateEdge(State::INIT, State::END, ';');
  registerStateEdge(State::INIT, State::END, '[');
  registerStateEdge(State::INIT, State::END, ']');
  registerStateEdge(State::INIT, State::END, '&');

  string line;
  vector<Token> tokens;

  while (getline(cin, line)) {
    if (line.find("//") != string::npos) {
      line = line.substr(0, line.find("//"));
    }

    if (line == "") {
      continue;
    }

    if (!scan(line, tokens)) {
      cerr << "ERROR";
      return 1;
    }
  }

  if (!ensureCorrectness(tokens)) {
    cerr << "ERROR";
    return 1;
  }

  for (int i = 0; i < tokens.size(); ++i) {
    if (tokens.at(i).getKind() != TokenKind::SPACE) {
      cout << tokens.at(i).toString() << endl;
    }
  }

  return 0;
}
