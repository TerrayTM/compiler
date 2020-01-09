//----------------------------------------------------------
//| Assembler to convert MIPS instructions to machine code |
//----------------------------------------------------------

#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <map>
#include <limits>
#include <unordered_set>
#include "scanner.h"

using namespace std;

enum IDFormat {
  OneRegisterGroupOne,
  OneRegisterGroupTwo,
  TwoRegisters,
  ThreeRegisters,
  Branch,
  StoreLoad
};

// Maps instruction string to IDFormat and its base machine code
map<string, pair<IDFormat, unsigned int>> instructions {
  // Instructions that take one register as operand group one
  {"jr", pair<IDFormat, unsigned int>(IDFormat::OneRegisterGroupOne, 8)},
  {"jalr", pair<IDFormat, unsigned int>(IDFormat::OneRegisterGroupOne, 9)},
  
  // Instructions that take one register as operand group two
  {"lis", pair<IDFormat, unsigned int>(IDFormat::OneRegisterGroupTwo, 20)},
  {"mflo", pair<IDFormat, unsigned int>(IDFormat::OneRegisterGroupTwo, 18)},
  {"mfhi", pair<IDFormat, unsigned int>(IDFormat::OneRegisterGroupTwo, 16)},

  // Instructions that take three registers as operand
  {"add", pair<IDFormat, unsigned int>(IDFormat::ThreeRegisters, 32)},
  {"sub", pair<IDFormat, unsigned int>(IDFormat::ThreeRegisters, 34)},
  {"slt", pair<IDFormat, unsigned int>(IDFormat::ThreeRegisters, 42)},
  {"sltu", pair<IDFormat, unsigned int>(IDFormat::ThreeRegisters, 43)},

  // Instructions that take two registers and an integer/label as operand
  {"beq", pair<IDFormat, unsigned int>(IDFormat::Branch, 268435456)},
  {"bne", pair<IDFormat, unsigned int>(IDFormat::Branch, 335544320)},
  
  // Instructions that take two registers as operand
  {"mult", pair<IDFormat, unsigned int>(IDFormat::TwoRegisters, 24)},
  {"multu", pair<IDFormat, unsigned int>(IDFormat::TwoRegisters, 25)},
  {"div", pair<IDFormat, unsigned int>(IDFormat::TwoRegisters, 26)},
  {"divu", pair<IDFormat, unsigned int>(IDFormat::TwoRegisters, 27)},

  // Instructions that take a special set of parameters
  {"sw", pair<IDFormat, unsigned int>(IDFormat::StoreLoad, 2885681152)},
  {"lw", pair<IDFormat, unsigned int>(IDFormat::StoreLoad, 2348810240)}
};

// Helper class for storing tokens and processing them one by one
class Tokens {
  public:
    Tokens(vector<Token> base) : tokens(base), index(0) {}

    Token getCurrent() const {
      return tokens.at(index);
    }

    bool hasCurrent() const {
      return tokens.size() > index;
    }

    bool isEmpty() const {
      return tokens.size() == 0;
    }

    // Removes all labels from tokens and return them as an vector
    vector<string> removeLabels() {
      vector<string> labels;

      for (int i = 0; i < tokens.size(); ++i) {
        Token& current = tokens.at(i);

        if (current.getKind() == Token::Kind::LABEL) {
          string label = current.getLexeme();

          // Strip the colon character off the label
          label = label.substr(0, label.size() - 1);
          labels.push_back(label);
          tokens.erase(tokens.begin() + i--);
        }
      }

      return labels;
    }

    // Removes all commas and brackets from tokens
    void removeUselessElements() {
      for (int i = 0; i < tokens.size(); ++i) {
        Token::Kind kind = tokens.at(i).getKind();

        if (kind == Token::Kind::COMMA || kind == Token::Kind::LPAREN || kind == Token::Kind::RPAREN) {
          tokens.erase(tokens.begin() + i--);
        }
      }
    }

    void reset() {
      index = 0;
    }

    void increment() {
      ++index;
    }

    // Checks if the current token is a left bracket
    bool isCurrentLeftBracket() const {
      return index < tokens.size() && tokens.at(index).getKind() == Token::Kind::LPAREN;
    }

    // Checks if the current token is a right bracket
    bool isCurrentRightBracket() const {
      return index < tokens.size() && tokens.at(index).getKind() == Token::Kind::RPAREN;
    }

    // Checks if the current token is a comma
    bool isCurrentComma() const {
        return index < tokens.size() && tokens.at(index).getKind() == Token::Kind::COMMA;
    }

    // Checks if the current token is a valid symbol
    bool isCurrentSymbol() const {
        return index < tokens.size() && tokens.at(index).getKind() == Token::Kind::ID;
    }

    // Checks if current token is a valid register
    bool isCurrentRegister() const {
      if (index < tokens.size()) {
        Token current = tokens.at(index);

        if (current.getKind() == Token::Kind::REG) {
          string candidate = current.getLexeme();

          if (candidate.size() >= 2 && candidate.size() < 4) {
            // Strip off the dollar sign symbol
            candidate = candidate.substr(1, candidate.size() - 1);

            // Check if register is a valid register
            int integer = stoi(candidate);
            return integer <= 31 && integer >= 0;
          } 
        }
      }

      return false;
    }

    // Checks if current token is a valid number
    // Can be in hex or decimal form
    bool isCurrentNumber(bool useSmallNumber = false) const {
      if (index < tokens.size()) {
        Token current = tokens.at(index);

        if (current.getKind() == Token::Kind::INT) {
          string candidate = current.getLexeme();

          if (candidate.size() < 12) {
            long long integer = stoll(candidate);

            // Small number flag tests for 16 bigs if set true
            if (useSmallNumber) {
              return integer <= numeric_limits<short>::max() && integer >= numeric_limits<short>::min();
            } else {
              return integer <= numeric_limits<unsigned int>::max() && integer >= numeric_limits<int>::min();
            }
          }
        } else if (current.getKind() == Token::Kind::HEXINT) {
          return useSmallNumber ? current.getLexeme().size() <= 6 : current.getLexeme().size() <= 10;
        }
      }

      return false;
    }

    // Checks if current token is the last one in the list
    bool isCurrentNothing() const {
      return index >= tokens.size();
    }

  private:
    vector<Token> tokens;
    int index;
};

int translateLine(Tokens& tokenLine, map<string, unsigned int>& symbols, bool& success, int location);

bool isValidLine(Tokens& tokenLine);

bool transformLine(Tokens& tokenLine);

const string ERROR = "ERROR";

int main() {
  string line;
  unsigned int counter = 0;
  vector<Tokens> intermediate;
  map<string, unsigned int> symbols;

  try {
    // First pass do analysis on input. Checks for errors and
    // outputs intermediate representation and the symbols table
    while (getline(cin, line)) {
      Tokens tokenLine(scan(line));

      if (!isValidLine(tokenLine)) {
        cerr << ERROR;
        break;
      }

      // Reset the sleek back to beginning for tokens
      tokenLine.reset();

      // Remove and return all the labels on a line
      vector<string> labels = tokenLine.removeLabels();

      // Add the labels to symbols map. If two labels of the same
      // name exists, throw error and exit 
      for (int i = 0; i < labels.size(); ++i) {
        string label = labels.at(i);

        if (symbols.find(label) == symbols.end()) {
          symbols[label] = counter;
        } else {
          cerr << ERROR;
          break;
        }
      }

      bool skipLine = transformLine(tokenLine);

      // Skip incrementing program counter and adding tokenLine to 
      // intermediate representation if it is empty
      if (!skipLine) {
        counter += 4;
        intermediate.push_back(tokenLine);
      }
    }

    // Second pass do synthesis. Converts labels to addresses
    // and ASCII instructions to machine code. Outputs machine code
    for (int i = 0, length = intermediate.size(); i < length; ++i) {
      bool success = true;
      unsigned int machineCode = translateLine(intermediate.at(i), symbols, success, i * 4);

      // Translation error occurred, stop and exit
      if (!success) {
        cerr << ERROR;
        break;
      }
      
      cout << char(machineCode >> 24) << char(machineCode >> 16) << char(machineCode >> 8) << char(machineCode);
    }
  } catch (ScanningFailure &f) {
    std::cerr << f.what() << std::endl;
    
    return 1;
  }

  return 0;
}

bool isValidLine(Tokens& tokenLine) {
  bool result = true;

  if (tokenLine.hasCurrent()) {
    Token head = tokenLine.getCurrent();
    tokenLine.increment();

    switch (head.getKind()) {
      case(Token::Kind::WORD):
        // .word is followed by only a number or a symbol
        result &= tokenLine.isCurrentNumber() || tokenLine.isCurrentSymbol();
        tokenLine.increment();
        result &= tokenLine.isCurrentNothing();
        break;
      
      case(Token::Kind::LABEL):
        // A label can be followed by another label
        // or any valid instruction sequence
        result &= isValidLine(tokenLine);
        break;

      case(Token::Kind::ID):
        // Invalid instruction name
        if (instructions.find(head.getLexeme()) == instructions.end()) {
          return false;
        }

        switch (instructions[head.getLexeme()].first)
        {
          case(IDFormat::OneRegisterGroupTwo):
          case(IDFormat::OneRegisterGroupOne):
            // This type of instruction must be followed by 1 register
            result &= tokenLine.isCurrentRegister();
            break;
          
          case(IDFormat::ThreeRegisters):
            // This type of instruction must be followed by 3 registers
            result &= tokenLine.isCurrentRegister();
            tokenLine.increment();
            result &= tokenLine.isCurrentComma();
            tokenLine.increment();
            
            // No break statement so that the three registers block flows into the two block
          case(IDFormat::TwoRegisters):
            result &= tokenLine.isCurrentRegister();
            tokenLine.increment();
            result &= tokenLine.isCurrentComma();
            tokenLine.increment();
            result &= tokenLine.isCurrentRegister();
            break;

          case(IDFormat::Branch):
            // This type of instruction must be followed by 2 registers and integer/label
            result &= tokenLine.isCurrentRegister();
            tokenLine.increment();
            result &= tokenLine.isCurrentComma();
            tokenLine.increment();
            result &= tokenLine.isCurrentRegister();
            tokenLine.increment();
            result &= tokenLine.isCurrentComma();
            tokenLine.increment();
            result &= tokenLine.isCurrentNumber(true) || tokenLine.isCurrentSymbol();
            break;

          case(IDFormat::StoreLoad):
            // This type of instruction must be followed by 1 register,
            // an integer, and another register in brackets
            result &= tokenLine.isCurrentRegister();
            tokenLine.increment();
            result &- tokenLine.isCurrentComma();
            tokenLine.increment();
            result &= tokenLine.isCurrentNumber(true);
            tokenLine.increment();
            result &= tokenLine.isCurrentLeftBracket();
            tokenLine.increment();
            result &= tokenLine.isCurrentRegister();
            tokenLine.increment();
            result &= tokenLine.isCurrentRightBracket();
            break;
        }

        // The next token must be empty
        tokenLine.increment();
        result &= tokenLine.isCurrentNothing();

        break;

      default:
        return false;
    }

    return result;
  }

  return true;
}

bool transformLine(Tokens& tokenLine) {
  // Remove any non-essential symbols for easier translation
  tokenLine.removeUselessElements();
  return tokenLine.isEmpty();
}

unsigned int resolveSymbol(string symbol, bool& success, map<string, unsigned int>& symbols) {
  if (symbols.find(symbol) != symbols.end()) {
    return symbols[symbol];
  } else {
    success = false;
  }

  return -1;
}

int translateLine(Tokens& tokenLine, map<string, unsigned int>& symbols, bool& success, int location) {
  unsigned int instruction = 0;
  Token head = tokenLine.getCurrent();
  tokenLine.increment();
  Token current = tokenLine.getCurrent();

  switch (head.getKind()) {
    case(Token::Kind::WORD):
      if (current.getKind() == Token::Kind::ID) {
        instruction = resolveSymbol(current.getLexeme(), success, symbols);
      } else {
        instruction = (unsigned int)bitset<32>(tokenLine.getCurrent().toLong()).to_ulong();
      }
      break;

    case(Token::Kind::ID):
      pair<IDFormat, unsigned int> info = instructions[head.getLexeme()];
      instruction = info.second;

      switch (info.first) {
        case(IDFormat::OneRegisterGroupOne): {
          unsigned int paramOne = current.toLong();
          instruction |= paramOne << 21;
          break;
        }
        
        case(IDFormat::ThreeRegisters): {
          unsigned int paramOne = current.toLong();
          tokenLine.increment();
          unsigned int paramTwo = tokenLine.getCurrent().toLong();
          tokenLine.increment();
          unsigned int paramThree = tokenLine.getCurrent().toLong();
          instruction |= (paramOne << 11) | (paramThree << 16) | (paramTwo << 21);
          break;
        }

        case(IDFormat::Branch): {
          unsigned int paramOne = current.toLong();
          tokenLine.increment();
          unsigned int paramTwo = tokenLine.getCurrent().toLong();
          tokenLine.increment();
          Token variable = tokenLine.getCurrent();
          unsigned int paramThree = 0;

          if (variable.getKind() == Token::Kind::ID) {
            int skipCount = ((int)resolveSymbol(variable.getLexeme(), success, symbols) - location - 4) / 4;
            
            // Make sure the new calculated skip number fits in 16 bits
            if (skipCount <= numeric_limits<short>::max() && skipCount >= numeric_limits<short>::min()) {
              paramThree = (unsigned int)bitset<16>(skipCount).to_ulong();
            } else {
              success = false;
            }
          } else {
            paramThree = (unsigned int)bitset<16>(variable.toLong()).to_ulong();
          }

          instruction |= (paramOne << 21) | (paramTwo << 16) | paramThree;
          break;
        }

        case(IDFormat::OneRegisterGroupTwo): {
          unsigned int paramOne = current.toLong();
          instruction |= paramOne << 11;
          break;
        }

        case(IDFormat::TwoRegisters): {
          unsigned int paramOne = current.toLong();
          tokenLine.increment();
          unsigned int paramTwo = tokenLine.getCurrent().toLong();
          instruction |= (paramOne << 21) | (paramTwo << 16);
          break;
        }

        case(IDFormat::StoreLoad): {
          unsigned int paramOne = current.toLong();
          tokenLine.increment();
          unsigned int paramTwo = (unsigned int)bitset<16>(tokenLine.getCurrent().toLong()).to_ulong();
          tokenLine.increment();
          unsigned int paramThree = tokenLine.getCurrent().toLong();
          instruction |= (paramOne << 16) | (paramThree << 21) | paramTwo;
          break;
        }
      }
      break;
  }

  return instruction;
}
