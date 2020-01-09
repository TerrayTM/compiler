//---------------------------------------------------------------------
//| Parser to check ordering of tokens using LR(1) syntactic analysis |
//---------------------------------------------------------------------

#include <iostream>
#include <string>
#include <stack>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <fstream>

using namespace std;

struct Node {
	string value;
	string lexeme;
	vector<Node> children;
};

void traverse(Node node) {
	cout << node.value;

	if (node.children.empty()) {
		cout << " " << node.lexeme << endl;

		return;
	}
	
	for (int i = 0; i < node.children.size(); ++i) {
		if (i == 0) {
			cout << " ";
		}

		cout << node.children[i].value;
		
		if (i + 1 != node.children.size()) {
			cout << " ";
		}
	}

	cout << endl;

	for (int i = 0; i < node.children.size(); ++i) {
		traverse(node.children[i]);
	}
}

int main() {
	vector<string> lines;
	string line;
	ifstream grammar("grammar.txt");

	getline(grammar, line);
	int count = stoi(line);

	for (int i = 0; i < count; ++i) {
		getline(grammar, line);
	}

	getline(grammar, line);
	count = stoi(line);

	for (int i = 0; i < count; ++i) {
		getline(grammar, line);
	}

	getline(grammar, line);

	getline(grammar, line);
	count = stoi(line);

	unordered_map<int, pair<string, int>> rules;
	vector<string> productionRules;

	for (int i = 0; i < count; ++i) {
		getline(grammar, line);

		productionRules.push_back(line);
		istringstream iss(line);
		vector<string> tokens{ istream_iterator<string>{iss}, istream_iterator<string>{} };

		rules.insert({ i, pair<string, int> {tokens[0], tokens.size() - 1} });
	}

	getline(grammar, line);
	int statesCount = stoi(line);

	getline(grammar, line);
	count = stoi(line);

	unordered_map<int, unordered_map<string, int>> reduces;
	unordered_map<int, unordered_map<string, int>> shifts;

	for (int i = 0; i < count; ++i) {
		getline(grammar, line);

		istringstream iss(line);
		vector<string> tokens{ istream_iterator<string>{iss}, istream_iterator<string>{} };

		int state = stoi(tokens[0]);
		string token = tokens[1];
		string action = tokens[2];
		int free = stoi(tokens[3]);

		if (action == "reduce") {
			if (reduces.find(state) == reduces.end()) {
				reduces.insert({ state, unordered_map<string, int>{ { token, free } } });
			}
			else {
				reduces[state].insert({ token, free });
			}
		}
		else {
			if (shifts.find(state) == shifts.end()) {
				shifts.insert({ state, unordered_map<string, int>{ { token, free } } });
			}
			else {
				shifts[state].insert({ token, free });
			}
		}
	}

	string token;
	vector<pair<string, string>> tokens;

	tokens.push_back({ "BOF", "BOF" });

	while (std::cin >> token) {
		string lexeme;
		std::cin >> lexeme;
		tokens.push_back({ token, lexeme });
	}

	tokens.push_back({ "EOF", "EOF" });

	if (tokens.empty()) {
		return 0;
	}

	Node start;
	start.value = tokens[0].first;
	start.lexeme = tokens[0].second;

	stack<Node> symbols{ { start } };
	stack<int> states{ { shifts[0][tokens[0].first] } };
	bool failed = false;

	for (int i = 1; i < tokens.size(); ++i) {
		while (true) {
			if (reduces.find(states.top()) != reduces.end()) {
				unordered_map<string, int> &scratch = reduces[states.top()];

				if (scratch.find(tokens[i].first) != scratch.end()) {
					int rule = scratch[tokens[i].first];
					pair<string, int> production = rules[rule];
					vector<Node> nodes;

					for (int j = 0; j < production.second; ++j) {
						nodes.insert(nodes.begin(), symbols.top());
						symbols.pop();
						states.pop();
					}

					Node next;
					next.value = production.first;
					next.children = nodes;

					symbols.push(next);
					states.push(shifts[states.top()][production.first]);

					continue;
				}
			}

			break;
		}

		Node next;
		next.value = tokens[i].first;
		next.lexeme = tokens[i].second;

		symbols.push(next);

		unordered_map<string, int> &scratch = shifts[states.top()];

		if (scratch.find(tokens[i].first) == scratch.end()) {
			cerr << "ERROR at " << i << endl;

			failed = true;

			break;
		}
		else {
			states.push(scratch[tokens[i].first]);
		}
	}

	if (!failed && symbols.top().value == "EOF") {
		Node start;
		start.value = "start";

		while (!symbols.empty()) {
			start.children.insert(start.children.begin(), symbols.top());
			symbols.pop();
		}

		traverse(start);
	}

	return 0;
}
