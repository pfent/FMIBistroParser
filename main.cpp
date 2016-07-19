#include <sstream>
#include <regex>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cpr/cpr.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

using namespace std;

auto getSpeiseplan() {
    const auto weekNumber = boost::posix_time::second_clock::local_time().date().week_number();
    const auto baseUrl = string{"http://www.betriebsrestaurant-gmbh.de/"};
    const auto index = string{"index.php?id=91"};
    const auto pdfRegex = regex{string{"<a href=\"(.*KW"} + to_string(weekNumber) + string{".+_FMI_.*\\.pdf)\" "}};

    const vector<string> pdfs = [baseUrl, index, pdfRegex]() {
        auto res = vector<string>{};
        auto response = cpr::Get(cpr::Url{baseUrl + index});
        if (response.status_code != 200) {
            return res;
        }
        auto stream = stringstream{response.text};
        for (string line; getline(stream, line);) {
            smatch match;
            if (regex_search(line, match, pdfRegex) && match.size() == 2) {
                res.push_back(match[1]);
            }
        }
        return res;
    }();

    auto result = string{};
    for (const auto &pdf : pdfs) {
        const auto response = cpr::Get(cpr::Url{baseUrl + pdf});
        auto bytes = vector<char>{response.text.begin(), response.text.end()};
        auto doc = unique_ptr<poppler::document>(poppler::document::load_from_data(&bytes));
        const auto pages = doc->pages();
        for (int i = 0; i < pages; ++i) {
            const auto page = unique_ptr<poppler::page>(doc->create_page(i));
            const auto text = page->text().to_utf8();
            result += string(text.begin(), text.end());
        }
    }
    return result;
}

auto parseMeals(const string &data) {
    auto mealPlan = map<string, vector<pair<string, double>>>{};
    auto stream = stringstream{data};
    auto currentDay = string{};
    auto currentMeals = vector<pair<string, double>>{};
    const auto daysRegex = regex{"^[Montag|Dienstag|Mittwoch|Donnerstag|Freitag]"};
    const auto mealRegex = regex{"^\\d+\\.\\s*(.*) (\\d\\.\\d\\d)"};
    for (string line; getline(stream, line);) {
        auto match = smatch{};
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
    const auto header = regex{"([\\s\\S]*Tagessuppe.*)"};
    const auto footer = regex{"(Enthält laut neuer Lebensmittel-Informationsverordnung:[\\s\\S]*)"};

    raw = regex_replace(raw, header, "");
    raw = regex_replace(raw, footer, "");
    raw = regex_replace(raw, regex{"(\n )"}, " "); // some lines have random newlines in them
    raw = regex_replace(raw, regex{"oder B.n.W."}, ""); // Beilage nach Wahl
    raw = regex_replace(raw, regex{"\\*"}, ""); // random asterisks
    raw = regex_replace(raw, regex{" \\d(,\\d)* "}, ""); // additives "1,2,3"

    raw = regex_replace(raw, regex{"( +)"}, " "); // remove superfluous whitespace

    auto mealPlan = parseMeals(raw);
    for (const auto &day : mealPlan) {
        cout << day.first << " has following meals:\n";
        for (const auto &meal : day.second) {
            cout << meal.first << " for " << meal.second << "€\n";
        }
    }

}