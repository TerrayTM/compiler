//----------------------------------------------------------
//| Program for semantic analysis and MIPS code generation |
//----------------------------------------------------------

#include <iostream>
#include <string>
#include <unordered_set>
#include <iterator>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace std;

unordered_set<string> terminals = {
    "BOF", "BECOMES", "COMMA", "ELSE", 
    "EOF", "EQ", "GE", "GT", "ID", 
    "IF", "INT", "LBRACE", "LE", "LPAREN", 
    "LT", "MINUS", "NE", "NUM", "PCT", 
    "PLUS", "PRINTLN", "RBRACE", "RETURN", 
    "RPAREN", "SEMI", "SLASH", "STAR", 
    "WAIN", "WHILE", "AMP", "LBRACK", 
    "RBRACK", "NEW", "DELETE", "NULL"
};

enum Type {
    INT,
    INTSTAR,
    UNDEF
};

enum PartialType {
    LOCATION,
    NUMBER,
    CODE
};

struct Scope {
    vector<pair<string, Type>> parameters;
    unordered_map<string, tuple<Type, int>> variables;
    int order;
    int locationCount;
    bool parametersLoaded;
    int variablesCount;

    void importParameters() {
        if (!parametersLoaded) {
            parametersLoaded = true;
            variablesCount = variables.size() - parameters.size();

            for (int i = 0, length = parameters.size(); i < length; ++i) {
                if (parameters.at(i).first != "") {
                    variables[parameters.at(i).first] = make_tuple(parameters.at(i).second, (length - i) * 4);
                }
            }
        }
    }
};

struct Node {
    string head;
    vector<string> tokens;
    vector<Node> children;
    string rule;
    tuple<string, PartialType> partial = make_tuple("", PartialType::CODE);
    Type type;
    vector<Type> signature;
};

unordered_map<string, Type> typeMapping = {
    { "type INT", Type::INT }, { "type INT STAR", Type::INTSTAR }  
};

const string MAIN_PROCEDURE = "main INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE";
const string PROCEDURE = "procedure INT ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE";

Node createParseTree(vector<tuple<string, string, vector<string>>>& input, int position, int& childrenCount) {
    tuple<string, string, vector<string>> current = input.at(position);
    int length = get<2>(current).size() - 1;
    int counter = position + 1;
    
    Node node;
    node.head = get<0>(current);
    node.rule = get<1>(current);
    node.tokens = get<2>(current);
    node.type = Type::UNDEF;

    ++childrenCount;

    if (terminals.find(get<0>(current)) != terminals.end()) {
        return node;
    }

    while (length --> 0) {
        int childs = 0;
        node.children.push_back(createParseTree(input, counter, childs));
        childrenCount += childs;
        counter += childs;
    }

    return node;
}

vector<tuple<string, string, vector<string>>> acquireInput() {
    string line;
    vector<tuple<string, string, vector<string>>> input;

    while (getline(cin, line)) {
        istringstream stream(line);
		vector<string> tokens { istream_iterator<string> { stream }, istream_iterator<string> {} };
        
        input.push_back(make_tuple(tokens.at(0), line, tokens));
    }

    return input;
}

bool manageScope(Node& head, unordered_map<string, Scope>& symbols, string& scope, int order) {
    if (head.rule == MAIN_PROCEDURE) {
        scope = "wain";
        
        if (symbols.find(scope) != symbols.end()) {
            return false;
        }

        Scope scopeInfo;
        scopeInfo.order = order;
        scopeInfo.locationCount = 0;
        scopeInfo.parametersLoaded = false;

        symbols.insert({ scope, scopeInfo });
    } else if (head.rule == PROCEDURE) {
        scope = head.children[1].tokens[1];

        if (symbols.find(scope) != symbols.end()) {
            return false;
        }
        
        Scope scopeInfo;
        scopeInfo.order = order;
        scopeInfo.locationCount = 0;
        scopeInfo.parametersLoaded = false;

        symbols.insert({ scope, scopeInfo });
    }

    return true;
}

bool checkDeclaration(Node& head, unordered_map<string, Scope>& symbols, string scope, bool scopeVariable, int order) {
    if (!manageScope(head, symbols, scope, order)) {
        return false;
    }
    
    if (head.rule == "params paramlist") {
        scopeVariable = true;
    } else if (head.rule == MAIN_PROCEDURE) {
        symbols[scope].parameters.push_back({"", typeMapping[head.children.at(3).children.at(0).rule]});
        symbols[scope].parameters.push_back({"", typeMapping[head.children.at(5).children.at(0).rule]});
    }

    if (head.rule == "dcl type ID") {
        Node& typeNode = head.children.at(0);
        Node& ID = head.children.at(1);

        if (typeNode.head == "ID") {
            Node& temporary = typeNode;
            typeNode = ID;
            ID = temporary;
        }

        Scope& currentScope = symbols[scope];
        string lexeme = ID.tokens.at(1);
        Type type = typeMapping[typeNode.rule];

        if (currentScope.variables.find(lexeme) == currentScope.variables.end()) {
            currentScope.variables.insert({lexeme, make_pair(type, scopeVariable ? -1 : -4 * currentScope.locationCount++)});

            if (scopeVariable) {
                currentScope.parameters.push_back({lexeme, type});
            }
        } else {
            return false;
        }
    } else {
        for (int i = 0; i < head.children.size(); ++i) {
            if (!checkDeclaration(head.children.at(i), symbols, scope, scopeVariable, order + 1)) {
                return false;
            }
        }
    }

    return true;
}

int countCommas(Node& head) {
    int count = 0;

    if (head.head == "COMMA") {
        ++count;
    }
    
    for (int i = 0; i < head.children.size(); ++i) {
        count += countCommas(head.children.at(i));
    }

    return count;
}

bool checkUndeclared(Node& head, unordered_map<string, Scope>& symbols, string scope) {
    manageScope(head, symbols, scope, 0);

    if (head.rule == "factor ID LPAREN RPAREN" || head.rule == "factor ID LPAREN arglist RPAREN") {
        string function = head.children.at(0).tokens.at(1);

        if (symbols[scope].variables.find(function) != symbols[scope].variables.end()) {
            return false;
        }

        if (symbols.find(function) == symbols.end() || symbols[function].order > symbols[scope].order) {
            return false;
        }

        if (head.tokens.size() == 5) {
            if (symbols[function].parameters.size() != countCommas(head.children.at(2)) + 1) {
                return false;
            }
        } else {
            if (symbols[function].parameters.size() > 0) {
                return false;
            }
        }
    } else if (head.rule == "factor ID" || head.rule == "lvalue ID") {
        string variableName = head.children.at(0).tokens.at(1);
        Scope currentScope = symbols[scope];

        if (currentScope.variables.find(variableName) == currentScope.variables.end()) {
            return false;
        }
    } else {
        for (int i = 0; i < head.children.size(); ++i) {
            if (!checkUndeclared(head.children.at(i), symbols, scope)) {
                return false;
            }
        }
    }

    return true;
}

bool checkType(Node& head, unordered_map<string, Scope>& symbols, string scope) {
    manageScope(head, symbols, scope, 0);

    for (int i = 0; i < head.children.size(); ++i) {
        if (!checkType(head.children.at(i), symbols, scope)) {
            return false;
        }
    }

    if (head.head == "NUM") {
        head.type = Type::INT;
    } else if (head.head == "NULL") {
        head.type = Type::INTSTAR;
    } else if (head.rule == "factor NUM") {
        head.type = Type::INT;
    } else if (head.rule == "factor NULL") {
        head.type = Type::INTSTAR;
    } else if (head.rule == "factor ID" || head.rule == "lvalue ID") {
        head.type = get<0>(symbols[scope].variables[head.children.at(0).tokens.at(1)]);
    } else if (head.rule == "factor ID LPAREN RPAREN" || head.rule == "factor ID LPAREN arglist RPAREN") {
        head.type = Type::INT;

        if (head.children.size() == 4) {
            vector<pair<string, Type>>& parameters = symbols[head.children.at(0).tokens.at(1)].parameters;

            for (int i = 0; i < parameters.size(); ++i) {
                if (head.children.at(2).signature.at(i) != parameters.at(i).second) {
                    return false;
                }
            }
        }
    } else if (head.rule == "arglist expr") {
        head.signature.push_back(head.children.at(0).type);
    } else if (head.rule == "arglist expr COMMA arglist") {
        vector<Type>& arguments = head.children.at(2).signature;

        head.signature.push_back(head.children.at(0).type);
        head.signature.insert(head.signature.end(), arguments.begin(), arguments.end());
    } else if (head.rule == "factor AMP lvalue") {
        if (head.children.at(1).type != Type::INT) {
            return false;
        }

        head.type = Type::INTSTAR;
    } else if (head.rule == "term factor" || head.rule == "expr term") {
        head.type = head.children.at(0).type;
    } else if (head.rule == "lvalue LPAREN lvalue RPAREN" || head.rule == "factor LPAREN expr RPAREN") {
        head.type = head.children.at(1).type;
    } else if (head.rule == "factor NEW INT LBRACK expr RBRACK") {
        if (head.children.at(3).type != Type::INT) {
            return false;
        }

        head.type = Type::INTSTAR;
    } else if (head.rule == "expr expr PLUS term") {
        Type first = head.children.at(0).type;
        Type second = head.children.at(2).type;

        if (first == second && first == Type::INTSTAR) {
            return false;
        }

        if (first == second) {
            head.type = Type::INT;
        } else {
            head.type = Type::INTSTAR;
        }
    } else if (head.rule == "expr expr MINUS term") {
        Type first = head.children.at(0).type;
        Type second = head.children.at(2).type;

        if (first == second && first == Type::INT) {
            head.type = Type::INT;
        } else if (first == Type::INTSTAR && second == Type::INT) {
            head.type = Type::INTSTAR;
        } else if (first == second && first == Type::INTSTAR) {
            head.type = Type::INT;
        } else {
            return false;
        }
    } else if (head.rule == "lvalue STAR factor" || head.rule == "factor STAR factor") {
        if (head.children.at(1).type != Type::INTSTAR) {
            return false;
        }

        head.type = Type::INT;
    } else if (head.rule == "term term STAR factor" || head.rule == "term term SLASH factor" || head.rule == "term term PCT factor") {
        if (head.children.at(0).type == head.children.at(2).type && head.children.at(2).type == Type::INT) {
            head.type = Type::INT;
        } else {
            return false;
        }
    } else if (head.head == "test") {
        if (head.children.at(0).type != head.children.at(2).type) {
            return false;
        }
    } else if (head.rule == "statement DELETE LBRACK RBRACK expr SEMI") {
        if (head.children.at(3).type != Type::INTSTAR) {
            return false;
        }
    } else if (head.rule == "statement PRINTLN LPAREN expr RPAREN SEMI") {
        if (head.children.at(2).type != Type::INT) {
            return false;
        }
    } else if (head.rule == "statement lvalue BECOMES expr SEMI") {
        if (head.children.at(0).type != head.children.at(2).type) {
            return false;
        }
    } else if (head.rule == "dcls dcls dcl BECOMES NULL SEMI" || head.rule == "dcls dcls dcl BECOMES NUM SEMI") {
        if (get<0>(symbols[scope].variables[head.children.at(1).children.at(1).tokens.at(1)]) != head.children.at(3).type) {
            return false;
        }
    } else if (head.rule == MAIN_PROCEDURE) {
        if (head.children.at(11).type != Type::INT) {
            return false;
        }

        if (head.children.at(5).children.at(0).tokens.size() > 2) {
            return false;
        }
        
        symbols["wain"].importParameters();
    } else if (head.rule == PROCEDURE) {
        if (head.children.at(9).type != Type::INT) {
            return false;
        }

        symbols[head.children.at(1).tokens.at(1)].importParameters();
    }

    return true;
}

void output(string line) {
    cout << line;
}

inline string subtractInstruction(string registerOne, string registerTwo, string registerThree) {
    return "sub " + registerOne + ", " + registerTwo + ", " + registerThree + "\n";
}

inline string loadInstruction(string registerOne, string offset, string base) {
    return "lw " + registerOne + ", " + offset + "(" + base + ")" + "\n";
}

inline string saveInstruction(string registerOne, string offset, string base) {
    return "sw " + registerOne + ", " + offset + "(" + base + ")" + "\n";
}

inline string addInstruction(string registerOne, string registerTwo, string registerThree) {
    return "add " + registerOne + ", " + registerTwo + ", " + registerThree + "\n";
}

inline string branchEqualInstruction(string registerOne, string registerTwo, string label) {
    return "beq " + registerOne + ", " + registerTwo + ", " + label + "\n";
}

inline string branchNotEqualInstruction(string registerOne, string registerTwo, string label) {
    return "bne " + registerOne + ", " + registerTwo + ", " + label + "\n";
}

inline string setLessThanInstruction(string registerOne, string registerTwo, string registerThree) {
    return "slt " + registerOne + ", " + registerTwo + ", " + registerThree + "\n";
}

inline string setLessThanUnsignedInstruction(string registerOne, string registerTwo, string registerThree) {
    return "sltu " + registerOne + ", " + registerTwo + ", " + registerThree + "\n";
}

inline string loadSkipInstruction(string registerOne, string value) {
    return "lis " + registerOne + "\n" + ".word " + value + "\n";
}

inline string multiplyInstruction(string registerOne, string registerTwo) {
    return "mult " + registerOne + ", " + registerTwo + "\n";
}

inline string moveLowInstruction(string registerOne) {
    return "mflo " + registerOne + "\n";
}

inline string moveHighInstruction(string registerOne) {
    return "mfhi " + registerOne + "\n";
}

inline string divideInstruction(string registerOne, string registerTwo) {
    return "div " + registerOne + ", " + registerTwo + "\n";
}

inline string pushInstruction(string registerValue) {
    return saveInstruction(registerValue, "-4", "$30") + subtractInstruction("$30", "$30", "$4");
}

inline string popInstruction(string registerValue) {
    return addInstruction("$30", "$30", "$4") + loadInstruction(registerValue, "-4", "$30");
}

inline string jumpInstruction(string registerOne) {
    return "jr " + registerOne + "\n";
}

inline string importInstruction(string module) {
    return ".import " + module + "\n";
}

inline string jumpLinkInstruction(string registerOne) {
    return "jalr " + registerOne + "\n";
}

inline string labelInstruction(string label) {
    return label + ":\n";
}

string code(tuple<string, PartialType> partial) {
    string parameter = get<0>(partial);

    switch (get<1>(partial)) {
        case PartialType::CODE: return parameter;
        case PartialType::NUMBER: return loadSkipInstruction("$3", parameter);
        case PartialType::LOCATION: return loadInstruction("$3", parameter, "$29");
    }
}

bool printIncluded = false;
int currentLabel = 0;

inline string generateLabel() {
    return "L" + to_string(currentLabel++);
}

inline string generateFunction(string name) {
    return "F" + name;
}

bool traverseSearchPointer(Node& head) {
    return head.rule == "lvalue LPAREN lvalue RPAREN" ? traverseSearchPointer(head.children.at(1)) : head.tokens.size() == 3;
}

void generateCode(Node& head, unordered_map<string, Scope>& symbols, string scope) {
    manageScope(head, symbols, scope, 0);

    for (int i = 0; i < head.children.size(); ++i) {
        generateCode(head.children.at(i), symbols, scope);
    }

    Scope& currentScope = symbols[scope];

    if (head.rule == MAIN_PROCEDURE) {
        string partialCode = "";

        partialCode += loadSkipInstruction("$4", "4");
        partialCode += loadSkipInstruction("$11", "1");
        partialCode += subtractInstruction("$29", "$30", "$4");
        partialCode += loadSkipInstruction("$12", to_string(currentScope.variablesCount * 4 + 8));
        partialCode += subtractInstruction("$30", "$30", "$12");
        partialCode += saveInstruction("$1", get<0>(head.children.at(3).partial), "$29");
        partialCode += saveInstruction("$2", get<0>(head.children.at(5).partial), "$29");

	if (head.children.at(3).children.at(0).tokens.size() == 2) {
            partialCode += loadSkipInstruction("$2", "0");
        }

        partialCode += importInstruction("init");
        partialCode += importInstruction("new");
        partialCode += importInstruction("delete");
        partialCode += pushInstruction("$31");
        partialCode += loadSkipInstruction("$10", "init");
        partialCode += jumpLinkInstruction("$10");
        partialCode += popInstruction("$31");
        partialCode += code(head.children.at(8).partial);
        partialCode += code(head.children.at(9).partial);
        partialCode += code(head.children.at(11).partial);
        partialCode += addInstruction("$30", "$29", "$4");
        partialCode += jumpInstruction("$31");

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "dcl type ID") {
	head.partial = make_pair(to_string(get<1>(currentScope.variables[head.children.at(1).tokens.at(1)])), PartialType::LOCATION);
    } else if (head.rule == "expr term" || head.rule == "term factor" || head.rule == "procedures main" || head.rule == "arglist expr" || head.rule == "factor NUM" || head.rule == "factor NULL") {
        head.partial = head.children.at(0).partial;
    } else if (head.rule == "factor ID" || head.rule == "lvalue ID") {
        head.partial = make_pair(to_string(get<1>(currentScope.variables[head.children.at(0).tokens.at(1)])), PartialType::LOCATION);
    } else if (head.rule == "factor LPAREN expr RPAREN" || head.rule == "lvalue LPAREN lvalue RPAREN" || head.rule == "lvalue STAR factor") {
        head.partial = head.children.at(1).partial;
    } else if (head.rule == "expr expr PLUS term" || head.rule == "expr expr MINUS term") {
        string partialCode = "";

        partialCode += code(head.children.at(0).partial);

        if (head.children.at(2).type == Type::INTSTAR && head.tokens.at(2) == "PLUS") {
            partialCode += multiplyInstruction("$3", "$4");
            partialCode += moveLowInstruction("$3");
        }

        partialCode += pushInstruction("$3");
        partialCode += code(head.children.at(2).partial);

        if (head.children.at(0).type == Type::INTSTAR && head.children.at(2).type == Type::INT) {
            partialCode += multiplyInstruction("$3", "$4");
            partialCode += moveLowInstruction("$3");
        }

        partialCode += popInstruction("$5");
        partialCode += head.tokens.at(2) == "PLUS" ? addInstruction("$3", "$5", "$3") : subtractInstruction("$3", "$5", "$3");

        if (head.tokens.at(2) == "MINUS" && head.children.at(2).type == Type::INTSTAR) {
            partialCode += divideInstruction("$3", "$4");
            partialCode += moveLowInstruction("$3");
        }

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "term term STAR factor" || head.rule == "term term SLASH factor" || head.rule == "term term PCT factor") {
        string partialCode = "";

        partialCode += code(head.children.at(0).partial);
        partialCode += pushInstruction("$3");
        partialCode += code(head.children.at(2).partial);
        partialCode += popInstruction("$5");
        partialCode += head.tokens.at(2) == "STAR" ? multiplyInstruction("$5", "$3") : divideInstruction("$5", "$3");
        partialCode += head.tokens.at(2) == "PCT" ? moveHighInstruction("$3") : moveLowInstruction("$3");

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "statements statements statement") {
        head.partial = make_tuple(code(head.children.at(0).partial) + code(head.children.at(1).partial), PartialType::CODE);
    } else if (head.rule == "statement PRINTLN LPAREN expr RPAREN SEMI") {
        string partialCode = "";

        if (!printIncluded) {
            partialCode += importInstruction("print");
            printIncluded = true;
        }

        partialCode += code(head.children.at(2).partial);
        partialCode += addInstruction("$1", "$3", "$0");
        partialCode += pushInstruction("$31");
        partialCode += loadSkipInstruction("$10", "print");
        partialCode += jumpLinkInstruction("$10");
        partialCode += popInstruction("$31");

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "statement lvalue BECOMES expr SEMI") {
        string partialCode = code(head.children.at(2).partial);

        if (traverseSearchPointer(head.children.at(0))) { 
            partialCode += pushInstruction("$3");
            partialCode += code(head.children.at(0).partial);
            partialCode += popInstruction("$5");
            partialCode += saveInstruction("$5", "0", "$3");
        } else {
            partialCode += saveInstruction("$3", get<0>(head.children.at(0).partial), "$29");
        }

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "dcls dcls dcl BECOMES NUM SEMI" || head.rule == "dcls dcls dcl BECOMES NULL SEMI") {
        head.partial = make_tuple(code(head.children.at(0).partial) + code(head.children.at(3).partial) + saveInstruction("$3", get<0>(head.children.at(1).partial), "$29"), PartialType::CODE);
    } else if (head.head == "NUM") {
        head.partial = make_tuple(head.tokens.at(1), PartialType::NUMBER);
    } else if (head.rule == "test expr LT expr" || head.rule == "test expr GT expr" || head.rule == "test expr GE expr" || head.rule == "test expr LE expr") {
        string partialCode = "";

        partialCode += code(head.children.at(0).partial);
        partialCode += pushInstruction("$3");
        partialCode += code(head.children.at(2).partial);
        partialCode += popInstruction("$5");

        if (head.tokens.at(2) == "LT" || head.tokens.at(2) == "GE") {
            partialCode += head.children.at(0).type == Type::INTSTAR ? setLessThanUnsignedInstruction("$3", "$5", "$3") : setLessThanInstruction("$3", "$5", "$3");
        } else {
            partialCode += head.children.at(0).type == Type::INTSTAR ? setLessThanUnsignedInstruction("$3", "$3", "$5") : setLessThanInstruction("$3", "$3", "$5");
        }

        if (head.tokens.at(2) == "GE" || head.tokens.at(2) == "LE") {
            partialCode += subtractInstruction("$3", "$11", "$3");
        }

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE") {
        string partialCode = "";
        string labelOne = generateLabel();
        string labelTwo = generateLabel();

        partialCode += labelInstruction(labelOne);
        partialCode += code(head.children.at(2).partial);
        partialCode += branchEqualInstruction("$3", "$0", labelTwo);
        partialCode += code(head.children.at(5).partial);
        partialCode += branchEqualInstruction("$0", "$0", labelOne);
        partialCode += labelInstruction(labelTwo);

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "test expr NE expr" || head.rule == "test expr EQ expr") {
        string partialCode = "";

        partialCode += code(head.children.at(0).partial);
        partialCode += pushInstruction("$3");
        partialCode += code(head.children.at(2).partial);
        partialCode += popInstruction("$5");
        partialCode += head.children.at(0).type == Type::INT ? setLessThanUnsignedInstruction("$6", "$3", "$5") : setLessThanInstruction("$6", "$3", "$5");
        partialCode += head.children.at(0).type == Type::INT ? setLessThanUnsignedInstruction("$7", "$5", "$3") : setLessThanInstruction("$7", "$5", "$3");
        partialCode += addInstruction("$3", "$6", "$7");

        if (head.tokens.at(2) == "EQ") {
            partialCode += subtractInstruction("$3", "$11", "$3");
        }

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE") {
        string partialCode = "";
        string labelOne = generateLabel();
        string labelTwo = generateLabel();

        partialCode += code(head.children.at(2).partial);
        partialCode += branchEqualInstruction("$3", "$0", labelOne);
        partialCode += code(head.children.at(5).partial);
        partialCode += branchEqualInstruction("$0", "$0", labelTwo);
        partialCode += labelInstruction(labelOne);
        partialCode += code(head.children.at(9).partial);
        partialCode += labelInstruction(labelTwo);

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "factor STAR factor") {
        head.partial = make_tuple(code(head.children.at(1).partial) + loadInstruction("$3", "0", "$3"), PartialType::CODE);
    } else if (head.head == "NULL") {
        head.partial = make_tuple(addInstruction("$3", "$0", "$11"), PartialType::CODE);
    } else if (head.rule == "factor AMP lvalue") {
        string partialCode = "";

		if (get<1>(head.children.at(1).partial) == PartialType::LOCATION) {
			partialCode += loadSkipInstruction("$3", get<0>(head.children.at(1).partial));
			partialCode += addInstruction("$3", "$3", "$29");
		}
		else {
			partialCode += code(head.children.at(1).children.at(1).partial);
		}

		head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "factor NEW INT LBRACK expr RBRACK" || head.rule == "statement DELETE LBRACK RBRACK expr SEMI") {
        string partialCode = "";
        string label = "";
        
        partialCode += code(head.children.at(3).partial);

        if (head.tokens.at(1) == "DELETE") {
            label = generateLabel();
            partialCode += branchEqualInstruction("$3", "$11", label);
        }
        
        partialCode += addInstruction("$1", "$0", "$3");
        partialCode += pushInstruction("$31");
        partialCode += loadSkipInstruction("$10", head.tokens.at(1) == "NEW" ? "new" : "delete");
        partialCode += jumpLinkInstruction("$10");
        partialCode += popInstruction("$31");

        if (head.tokens.at(1) == "NEW") {
            partialCode += branchNotEqualInstruction("$3", "$0", "1");
            partialCode += addInstruction("$3", "$0", "$11");
        } else {
            partialCode += labelInstruction(label);
        }

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == PROCEDURE) {
        string partialCode = "";
        
        partialCode += labelInstruction(generateFunction(head.children.at(1).tokens.at(1)));
        partialCode += subtractInstruction("$29", "$30", "$4");
        partialCode += loadSkipInstruction("$12", to_string(currentScope.variablesCount * 4));
        partialCode += subtractInstruction("$30", "$30", "$12");
        partialCode += code(head.children.at(6).partial);
        partialCode += code(head.children.at(7).partial);
        partialCode += code(head.children.at(9).partial);
        partialCode += addInstruction("$30", "$29", "$4");
        partialCode += jumpInstruction("$31");

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "procedures procedure procedures") {
        head.partial = make_tuple(code(head.children.at(1).partial) + code(head.children.at(0).partial), PartialType::CODE);
    } else if (head.rule == "factor ID LPAREN RPAREN" || head.rule == "factor ID LPAREN arglist RPAREN") {
        string partialCode = "";
        
        partialCode += pushInstruction("$29");
        partialCode += pushInstruction("$31");
        
        if (head.tokens.size() == 5) {
            partialCode += code(head.children.at(2).partial);
            partialCode += pushInstruction("$3");
        }

        partialCode += loadSkipInstruction("$10", generateFunction(head.children.at(0).tokens.at(1)));
        partialCode += jumpLinkInstruction("$10");
        
        if (head.tokens.size() == 5) {
            int argumentCount = countCommas(head.children.at(2)) + 1;

            partialCode += loadSkipInstruction("$12", to_string(4 * argumentCount));
            partialCode += addInstruction("$30", "$30", "$12");
        }

        partialCode += popInstruction("$31");
        partialCode += popInstruction("$29");

        head.partial = make_tuple(partialCode, PartialType::CODE);
    } else if (head.rule == "arglist expr COMMA arglist") {
        head.partial = make_tuple(code(head.children.at(0).partial) + pushInstruction("$3") + code(head.children.at(2).partial), PartialType::CODE);
    } else if (head.rule == "start BOF procedures EOF") {
        output(code(head.children.at(1).partial));
    }
 }

int main() {
    vector<tuple<string, string, vector<string>>> input = acquireInput();
    unordered_map<string, Scope> symbols;

    if (input.size() == 0) {
        return 0;
    }

    int childrenCount = 0;
    Node parseTree = createParseTree(input, 0, childrenCount);

    if (checkDeclaration(parseTree, symbols, "", false, 0)) {
        if (checkUndeclared(parseTree, symbols, "")) {
            if (checkType(parseTree, symbols, "")) {
                generateCode(parseTree, symbols, "");
                return 0;
            }
        }
    }
    
    cerr << "ERROR" << endl;
    return 0;
}
