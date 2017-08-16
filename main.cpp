#include <sstream>
#include <regex>
#include <boost/algorithm/string.hpp>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <curl/curl.h>
#include <fstream>
#include <iostream>

using namespace std;

// as per https://stackoverflow.com/a/9786295
static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *) userp)->append((char *) contents, size * nmemb);
    return size * nmemb;
}

auto download(const string &url) {
    std::string readBuffer;
    auto curl = curl_easy_init();
    if (curl == nullptr) {
        throw runtime_error{"couldn't init curl"};
    };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    auto res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        throw runtime_error{"downloading failed with: "s + curl_easy_strerror(res)};
    }
    curl_easy_cleanup(curl);

    return readBuffer;
}

auto getSpeiseplan() {
    const auto pdf = "https://mpi.fs.tum.de/files/speiseplan/speiseplan.pdf"s;

    const auto response = download(pdf);
    return vector<char>{response.begin(), response.end()};
}

auto pdfToString(vector<char> bytes) {
    auto result = string{};
    auto doc = unique_ptr<poppler::document>(poppler::document::load_from_data(&bytes));
    const auto pages = doc->pages();
    for (int i = 0; i < pages; ++i) {
        const auto page = unique_ptr<poppler::page>(doc->create_page(i));
        const auto text = page->text().to_utf8();
        result += string(text.begin(), text.end());
    }
    return result;
}

auto getLineSubstr(const string &line, size_t begin, size_t end = numeric_limits<size_t>::max()) {
    if (begin > line.size()) {
        return string{};
    }
    if (end > line.size()) {
        return line.substr(begin);
    }
    return line.substr(begin, end - begin);
}

auto parseMeals(const string &data) {
    const auto daysInWeek = 5;
    const auto daysRegex = regex{"(Montag).*(Dienstag).*(Mittwoch).*(Donnerstag).*(Freitag)"};
    auto stream = stringstream{data};
    const auto daysPoss = [&]() {
        vector<size_t> poss;
        for (string line; getline(stream, line);) {
            smatch match;
            if (regex_search(line, match, daysRegex) && match.size() == (1 + daysInWeek)) {
                for (size_t i = 1; i <= daysInWeek; ++i) {
                    poss.push_back(static_cast<size_t>(match.position(i)));
                }
                return poss; // early exit, so we can continue stream
            }
        }
        return poss;
    }();
    if (daysPoss.empty()) {
        throw runtime_error{"couldn't locate weekdays. maybe the structure of the PDF changed?"};
    }

    vector<string> res(daysInWeek);
    for (string line; getline(stream, line);) {
        for (size_t i = 0; i < daysPoss.size() - 1; ++i) {
            auto dayString = getLineSubstr(line, daysPoss[i], daysPoss[i + 1]);
            res[i].append(dayString).append("\n");
        }

        auto lastDay = daysPoss.size() - 1;
        auto lastDayString = getLineSubstr(line, daysPoss[lastDay]);
        res[lastDay].append(lastDayString).append("\n");
    }

    return res;
}

struct MenuItem {
    string description;
    string allergens;
    string price;
};

enum class DayParserState {
    Description, Allergenes, Price
};

// as per: https://stackoverflow.com/a/35302029
string removeAdditionalWhitespace(string input) {
    string res;
    unique_copy(input.begin(), input.end(), back_insert_iterator<string>(res), [](char a, char b) {
        return (isspace(a) != 0) && (isspace(b) != 0);
    });
    boost::trim(res);
    return res;
}

auto parseDay(const string &dayString) {
    auto res = vector<MenuItem>{};
    auto stream = stringstream{dayString};
    auto state = DayParserState::Description;
    auto currentItem = MenuItem{};

    for (string line; getline(stream, line);) {
        line = removeAdditionalWhitespace(line);

        if (line.find("Allergene") != string::npos) {
            state = DayParserState::Allergenes;
        } else if (line.find("€") != string::npos) {
            state = DayParserState::Price;
        }

        switch (state) {
            case DayParserState::Description:
                currentItem.description.append(line).append(" ");
                break;
            case DayParserState::Allergenes:
                currentItem.allergens.append(line).append(" ");
                break;
            case DayParserState::Price:
                currentItem.price = line;
                boost::trim(currentItem.description);
                boost::trim(currentItem.allergens);
                res.push_back(currentItem);
                currentItem = MenuItem{};
                state = DayParserState::Description;
                break;
        }
        if (res.size() >= 3) {
            break;
        }
    }
    return res;
}

auto readFile(const string &filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<char> content((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    return content;
}

int main(int argc, const char *argv[]) {
    vector<char> pdf;
    if (argc > 1) {
        pdf = readFile(argv[1]);
    } else {
        pdf = getSpeiseplan();
    }
    auto raw = pdfToString(pdf);
    if (raw.empty()) {
        cerr << "Error downloading\n";
        return -1;
    }

    auto mealPlan = parseMeals(raw);

    for (const auto &p : mealPlan) {
        auto dailyMenu = parseDay(p);
        for (auto meal : dailyMenu) {
            cout << meal.description << endl;
            cout << meal.allergens << endl;
            cout << meal.price << endl;
        }
        cout << "**********************************************************************" << endl;
    }
}