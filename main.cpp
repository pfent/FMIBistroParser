#include <sstream>
#include <regex>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cpr/cpr.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

using namespace std;

string getSpeiseplan() {
    const auto weekNumber = boost::posix_time::second_clock::local_time().date().week_number();
    const string baseUrl{"http://www.betriebsrestaurant-gmbh.de/"};
    const string index{"index.php?id=91"};
    const regex pdfRegex{string{"<a href=\"(.*KW"} + to_string(weekNumber) + string{".+_FMI_.*\\.pdf)\" "}};

    vector<string> pdfs;

    auto response = cpr::Get(cpr::Url{baseUrl + index});
    if (response.status_code != 200) {
        return nullptr;
    }
    stringstream stream{response.text};
    for (string line; getline(stream, line, '\n');) {
        smatch match;
        if (regex_search(line, match, pdfRegex) && match.size() == 2) {
            pdfs.push_back(match[1]);
        }
    }

    string result;
    for (const auto &pdf : pdfs) {
        stream.clear();
        response = cpr::Get(cpr::Url{baseUrl + pdf});
        vector<char> bytes{response.text.begin(), response.text.end()};
        unique_ptr<poppler::document> doc(poppler::document::load_from_data(&bytes));
        const auto pages = doc->pages();
        for (int i = 0; i < pages; ++i) {
            auto text = doc->create_page(i)->text().to_utf8();
            result += string(text.begin(), text.end());
        }
    }
    return result;
}

int main(int, const char **) {
    auto raw = getSpeiseplan();
    // c++ regex can't multiline match. [\s\S]* is the multiline match equivalent to .*
    const regex header{"([\\s\\S]*Tagessuppe.*)"};
    const regex footer{"(Enthält laut neuer Lebensmittel-Informationsverordnung:[\\s\\S]*)"};

    raw = regex_replace(raw, header, "");
    raw = regex_replace(raw, footer, "");

    cout << raw << '\n';

    tuple<int, int> asdf;

}