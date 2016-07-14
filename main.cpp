#include <sstream>
#include <regex>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cpr/cpr.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

using namespace std;

auto getSpeiseplan() {
    const auto weekNumber = boost::posix_time::second_clock::local_time().date().week_number();
    const string baseUrl{"http://www.betriebsrestaurant-gmbh.de/"};
    const string index{"index.php?id=91"};
    const regex pdfRegex{string{"<a href=\"(.*KW"} + to_string(weekNumber) + string{".+_FMI_.*\\.pdf)\" "}};

    vector<string> pdfs;

    auto response = cpr::Get(cpr::Url{baseUrl + index});
    if (response.status_code != 200) {
        return string{};
    }
    stringstream stream{response.text};
    for (string line; getline(stream, line);) {
        smatch match;
        if (regex_search(line, match, pdfRegex) && match.size() == 2) {
            pdfs.push_back(match[1]);
        }
    }

    string result;
    for (const auto &pdf : pdfs) {
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

auto parseMeals(const string &data) {
    map<string, vector<pair<string, double>>> mealPlan;
    stringstream stream(data);
    string currentDay{};
    vector<pair<string, double>> currentMeals{};
    const regex daysRegex{"^[Montag|Dienstag|Mittwoch|Donnerstag|Freitag]"};
    const regex mealRegex{"^\\d+\\.\\s*(.*) (\\d\\.\\d\\d)"};
    for (string line; getline(stream, line);) {
        smatch match;
        if (regex_search(line, daysRegex)) {
            if (currentMeals.size() != 0) {
                mealPlan[currentDay] = currentMeals;
            }
            currentDay = line;
            currentMeals.clear();
        } else if (regex_search(line, match, mealRegex)) {
            const auto meal = match[1];
            const auto price = boost::lexical_cast<double>(match[2]);
            currentMeals.push_back({meal, price});
        }
    }
    return mealPlan;
}

int main(int, const char **) {
    auto raw = getSpeiseplan();
    if (raw.empty()) {
        cerr << "Error downloading\n";
        return -1;
    }
    // c++ regex can't multiline match. [\s\S]* is the multiline match equivalent to .*
    const regex header{"([\\s\\S]*Tagessuppe.*)"};
    const regex footer{"(Enthält laut neuer Lebensmittel-Informationsverordnung:[\\s\\S]*)"};

    raw = regex_replace(raw, header, "");
    raw = regex_replace(raw, footer, "");
    raw = regex_replace(raw, regex{"(\n )"}, " "); // some lines have random newlines in them
    raw = regex_replace(raw, regex{"oder B.n.W."}, "");
    raw = regex_replace(raw, regex{"\\*"}, "");

    raw = regex_replace(raw, regex{"( +)"}, " "); // remove superfluous whitespace

    auto mealPlan = parseMeals(raw);

    for (const auto &day : mealPlan) {
        cout << day.first << " has following meals:\n";
        for (const auto &meal : day.second) {
            cout << meal.first << " for " << meal.second << "€\n";
        }
    }

}