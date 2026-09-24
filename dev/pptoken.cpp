// PA1 command-line adapter. Translation phases 1--3 are implemented by the
// shared preprocessing-token scanner used by posttoken.
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "preprocess/pp_tokenizer.h"
#include "preprocess/tokens/DebugPPTokenStream.h"

using namespace std;

namespace {

bool has_batch_stdin_arg(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    if (string(argv[i]) == "--batch-stdin")
      return true;
  return false;
}

int run_batch_mode() {
  string record;
  while (getline(cin, record)) {
    if (record.empty())
      continue;
    const size_t first_tab = record.find('\t');
    const size_t second_tab = first_tab == string::npos
                                  ? string::npos
                                  : record.find('\t', first_tab + 1);
    if (first_tab == string::npos || second_tab == string::npos ||
        record.find('\t', second_tab + 1) != string::npos) {
      cout << "EXIT_FAILURE\n";
      continue;
    }
    const string output_path = record.substr(0, first_tab);
    const string error_path =
        record.substr(first_tab + 1, second_tab - first_tab - 1);
    const string input_path = record.substr(second_tab + 1);
    ifstream input(input_path.c_str(), ios::binary);
    ofstream output(output_path.c_str(), ios::binary);
    if (!input || !output) {
      ofstream error(error_path.c_str());
      error << "ERROR: cannot read input\n";
      cout << "EXIT_FAILURE\n";
      continue;
    }
    ostringstream text;
    text << input.rdbuf();
    streambuf *saved = cout.rdbuf(output.rdbuf());
    int status = 0;
    try {
      DebugPPTokenStream stream;
      ScanPreprocessingTokens(text.str(), stream);
    } catch (const exception &error) {
      ofstream error_file(error_path.c_str());
      error_file << "ERROR: " << error.what() << endl;
      status = 1;
    }
    cout.rdbuf(saved);
    cout << (status ? "EXIT_FAILURE" : "EXIT_SUCCESS") << endl;
  }
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (has_batch_stdin_arg(argc, argv))
      return run_batch_mode();
    ostringstream text;
    text << cin.rdbuf();
    DebugPPTokenStream stream;
    ScanPreprocessingTokens(text.str(), stream);
    return EXIT_SUCCESS;
  } catch (const exception &error) {
    cerr << "ERROR: " << error.what() << endl;
    return EXIT_FAILURE;
  }
}
