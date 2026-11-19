// Inventory Manager (works with any C++ compiler incl. Dev-C++, no special settings needed)
// Build: g++ -Wall -Wextra -o inventory inventory.cpp
// Run:   inventory     (asks for the CSV path; transactions.csv is kept in the current folder)

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct Item {
    int id;
    std::string name;
    int quantity;
    double price;
};

enum Result { Ok, NotFound, DuplicateId, InsufficientStock, Overflow };

// ---------- helpers ----------
static char lowerChar(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), lowerChar);
    return s;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Safe string -> number conversions (whole string must be valid). These replace
// std::stoi / std::stod, which old MinGW compilers (Dev-C++) do not support well.
static bool parseInt(const std::string& text, int& out) {
    std::string t = trim(text);
    if (t.empty()) return false;
    char* end = 0;
    errno = 0;
    long v = std::strtol(t.c_str(), &end, 10);
    if (*end != '\0' || errno == ERANGE || v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}

// Accepts finite numbers >= 0 only.
static bool parsePrice(const std::string& text, double& out) {
    std::string t = trim(text);
    if (t.empty()) return false;
    char* end = 0;
    errno = 0;
    double v = std::strtod(t.c_str(), &end);
    if (*end != '\0' || errno == ERANGE) return false;
    if (v != v || v < 0 || v > DBL_MAX) return false;  // rejects NaN, negatives, inf
    out = v;
    return true;
}

static std::string intToStr(int v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

static std::string csvEscape(const std::string& f) {
    if (f.find_first_of(",\"") == std::string::npos) return f;
    std::string out = "\"";
    for (size_t i = 0; i < f.size(); ++i) {
        if (f[i] == '"') out += '"';
        out += f[i];
    }
    return out + "\"";
}

static std::vector<std::string> csvSplit(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                cur += '"';
                ++i;
            } else if (c == '"') {
                inQuotes = false;
            } else {
                cur += c;
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

static std::string timestamp() {
    std::time_t t = std::time(0);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

// ---------- Inventory ----------
class Inventory {
public:
    Inventory(const std::string& dataFile, const std::string& logFile)
        : dataFile_(dataFile), logFile_(logFile) {}

    void load() {
        std::ifstream in(dataFile_.c_str());
        if (!in) {
            std::cout << "No data file found (" << dataFile_
                      << "). Starting with an empty inventory.\n";
            return;
        }
        std::string line;
        int lineNo = 0, loaded = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            // Excel's "CSV UTF-8" format starts the file with a 3-byte marker; remove it.
            if (lineNo == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
                line = line.substr(3);
            // Ignore blank lines and empty Excel rows such as ",,,"
            if (line.find_first_not_of(", \t\r\n") == std::string::npos) continue;
            std::vector<std::string> f = csvSplit(line);
            int id, qty;
            double price;
            // Skip a header row such as: id,name,quantity,price
            if (lineNo == 1 && f.size() == 4 && !parseInt(f[0], id)) continue;
            if (f.size() != 4 || !parseInt(f[0], id) || !parseInt(f[2], qty) ||
                !parsePrice(f[3], price) || id <= 0 || qty < 0) {
                std::cout << "Warning: skipping invalid line " << lineNo << "\n";
                continue;
            }
            if (items_.count(id)) {
                std::cout << "Warning: duplicate ID " << id << " on line " << lineNo
                          << " skipped\n";
                continue;
            }
            Item item = {id, trim(f[1]), qty, price};
            items_[id] = item;
            ++loaded;
        }
        std::cout << "Loaded " << loaded << " item(s) from " << dataFile_ << ".\n";
    }

    bool save() const {
        std::ofstream out(dataFile_.c_str(), std::ios::trunc);
        if (!out) {
            std::cout << "Error: could not write to " << dataFile_ << "\n";
            return false;
        }
        out << "id,name,quantity,price\n";
        out << std::fixed << std::setprecision(2);
        for (std::map<int, Item>::const_iterator it = items_.begin();
             it != items_.end(); ++it) {
            const Item& m = it->second;
            out << m.id << ',' << csvEscape(m.name) << ',' << m.quantity << ','
                << m.price << '\n';
        }
        return true;
    }

    Result addItem(int id, const std::string& name, int qty, double price) {
        if (items_.count(id)) return DuplicateId;
        Item item = {id, name, qty, price};
        items_[id] = item;
        if (qty > 0) logTransaction(id, "INITIAL", qty, qty);
        return Ok;
    }

    // Empty name / negative price mean "leave unchanged".
    Result updateItem(int id, const std::string& newName, double newPrice) {
        std::map<int, Item>::iterator it = items_.find(id);
        if (it == items_.end()) return NotFound;
        if (!newName.empty()) it->second.name = newName;
        if (newPrice >= 0) it->second.price = newPrice;
        return Ok;
    }

    Result stockIn(int id, int qty) {
        std::map<int, Item>::iterator it = items_.find(id);
        if (it == items_.end()) return NotFound;
        if (qty > INT_MAX - it->second.quantity) return Overflow;
        it->second.quantity += qty;
        logTransaction(id, "STOCK-IN", qty, it->second.quantity);
        return Ok;
    }

    Result stockOut(int id, int qty) {
        std::map<int, Item>::iterator it = items_.find(id);
        if (it == items_.end()) return NotFound;
        if (qty > it->second.quantity) return InsufficientStock;  // never negative
        it->second.quantity -= qty;
        logTransaction(id, "STOCK-OUT", qty, it->second.quantity);
        return Ok;
    }

    const Item* find(int id) const {
        std::map<int, Item>::const_iterator it = items_.find(id);
        return it == items_.end() ? 0 : &it->second;
    }

    std::vector<const Item*> search(const std::string& query) const {
        std::vector<const Item*> found;
        std::string q = toLower(query);
        bool numeric = !q.empty();
        for (size_t i = 0; i < q.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(q[i]))) numeric = false;
        for (std::map<int, Item>::const_iterator it = items_.begin();
             it != items_.end(); ++it) {
            const Item& m = it->second;
            if ((numeric && intToStr(m.id) == q) ||
                toLower(m.name).find(q) != std::string::npos)
                found.push_back(&m);
        }
        return found;
    }

    void list() const {
        if (items_.empty()) {
            std::cout << "The inventory is empty.\n";
            return;
        }
        printHeader();
        double total = 0;
        for (std::map<int, Item>::const_iterator it = items_.begin();
             it != items_.end(); ++it) {
            printRow(it->second);
            total += it->second.quantity * it->second.price;
        }
        std::cout << std::string(64, '-') << "\nTotal stock value: " << std::fixed
                  << std::setprecision(2) << total << "\n";
    }

    void showHistory() const {
        std::ifstream in(logFile_.c_str());
        std::string line;
        if (!in) {
            std::cout << "No transactions recorded yet.\n";
            return;
        }
        std::cout << std::left << std::setw(21) << "Time" << std::setw(7) << "ID"
                  << std::setw(11) << "Type" << std::setw(8) << "Qty"
                  << "Balance\n" << std::string(55, '-') << "\n";
        bool any = false;
        while (std::getline(in, line)) {
            std::vector<std::string> f = csvSplit(line);
            if (f.size() != 5) continue;
            std::cout << std::left << std::setw(21) << f[0] << std::setw(7) << f[1]
                      << std::setw(11) << f[2] << std::setw(8) << f[3] << f[4] << "\n";
            any = true;
        }
        if (!any) std::cout << "No transactions recorded yet.\n";
    }

    static void printHeader() {
        std::cout << std::left << std::setw(6) << "ID" << std::setw(30) << "Name"
                  << std::setw(10) << "Qty" << std::setw(12) << "Price"
                  << "Value\n" << std::string(64, '-') << "\n";
    }

    static void printRow(const Item& m) {
        std::cout << std::left << std::setw(6) << m.id << std::setw(30)
                  << m.name.substr(0, 28) << std::setw(10) << m.quantity
                  << std::fixed << std::setprecision(2) << std::setw(12) << m.price
                  << m.quantity * m.price << "\n";
    }

private:
    // Append-only audit log: time,id,type,quantity,balance-after
    void logTransaction(int id, const std::string& type, int qty, int balance) {
        std::ofstream out(logFile_.c_str(), std::ios::app);
        if (!out) {
            std::cout << "Warning: could not write to " << logFile_ << "\n";
            return;
        }
        out << timestamp() << ',' << id << ',' << type << ',' << qty << ',' << balance
            << '\n';
    }

    std::string dataFile_, logFile_;
    std::map<int, Item> items_;  // ordered by ID
};

// ---------- input helpers ----------
static bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    return static_cast<bool>(std::getline(std::cin, out));
}

// minValue lets callers allow 0 (e.g. an opening quantity) or require >= 1.
static bool readInt(const std::string& prompt, int& value, int minValue = 1) {
    std::string s;
    while (readLine(prompt, s)) {
        int v;
        if (parseInt(s, v) && v >= minValue) {
            value = v;
            return true;
        }
        std::cout << "Please enter a whole number >= " << minValue << ".\n";
    }
    return false;
}

static bool readPrice(const std::string& prompt, double& value) {
    std::string s;
    while (readLine(prompt, s)) {
        if (parsePrice(s, value)) return true;
        std::cout << "Please enter a valid price (0 or more).\n";
    }
    return false;
}

static bool readNonEmpty(const std::string& prompt, std::string& out) {
    while (readLine(prompt, out)) {
        out = trim(out);
        if (!out.empty()) return true;
        std::cout << "This field cannot be empty.\n";
    }
    return false;
}

static void showMenu() {
    std::cout << "\n===== Inventory Manager =====\n"
              << " 1. Add an item\n"
              << " 2. Search items\n"
              << " 3. Update item (name / price)\n"
              << " 4. Stock-in (receive stock)\n"
              << " 5. Stock-out (remove stock)\n"
              << " 6. List all items\n"
              << " 7. View transaction history\n"
              << " 0. Save and exit\n";
}

int main() {
    std::string path;
    readLine("Inventory CSV file path (press Enter to use inventory.csv): ", path);
    path = trim(path);
    // "Copy as path" in Windows wraps the path in quotes; remove them.
    if (path.size() >= 2 && path[0] == '"' && path[path.size() - 1] == '"')
        path = path.substr(1, path.size() - 2);
    if (path.empty()) path = "inventory.csv";

    Inventory inv(path, "transactions.csv");
    inv.load();

    while (true) {
        showMenu();
        std::string s;
        if (!readLine("Choice: ", s)) break;  // EOF
        int choice;
        if (!parseInt(s, choice)) {
            std::cout << "Invalid choice. Enter a number from the menu.\n";
            continue;
        }
        if (choice == 0) break;

        switch (choice) {
            case 1: {
                int id, qty;
                double price;
                std::string name;
                if (!readInt("Item ID: ", id) || !readNonEmpty("Name: ", name) ||
                    !readInt("Opening quantity: ", qty, 0) ||
                    !readPrice("Unit price: ", price))
                    return 0;
                if (inv.addItem(id, name, qty, price) == Ok) {
                    inv.save();
                    std::cout << "Item added.\n";
                } else {
                    std::cout << "An item with ID " << id << " already exists.\n";
                }
                break;
            }
            case 2: {
                std::string q;
                if (!readNonEmpty("Search (ID or name): ", q)) return 0;
                std::vector<const Item*> results = inv.search(q);
                if (results.empty()) {
                    std::cout << "No matching items found.\n";
                } else {
                    Inventory::printHeader();
                    for (size_t i = 0; i < results.size(); ++i)
                        Inventory::printRow(*results[i]);
                }
                break;
            }
            case 3: {
                int id;
                if (!readInt("Item ID to update: ", id)) return 0;
                const Item* cur = inv.find(id);
                if (!cur) {
                    std::cout << "No item with ID " << id << ".\n";
                    break;
                }
                std::string name, priceStr;
                double price = -1;  // -1 = unchanged
                if (!readLine("New name (blank to keep '" + cur->name + "'): ", name))
                    return 0;
                name = trim(name);
                while (true) {
                    if (!readLine("New price (blank to keep): ", priceStr)) return 0;
                    if (trim(priceStr).empty()) break;
                    if (parsePrice(priceStr, price)) break;
                    std::cout << "Please enter a valid price (0 or more).\n";
                }
                inv.updateItem(id, name, price);
                inv.save();
                std::cout << "Item updated.\n";
                break;
            }
            case 4:
            case 5: {
                int id, qty;
                bool receiving = (choice == 4);
                if (!readInt("Item ID: ", id) ||
                    !readInt(receiving ? "Quantity received: " : "Quantity removed: ",
                             qty))
                    return 0;
                Result r = receiving ? inv.stockIn(id, qty) : inv.stockOut(id, qty);
                switch (r) {
                    case Ok:
                        inv.save();
                        std::cout << (receiving ? "Stock added" : "Stock removed")
                                  << ". New quantity: " << inv.find(id)->quantity << "\n";
                        break;
                    case NotFound:
                        std::cout << "No item with ID " << id << ".\n";
                        break;
                    case InsufficientStock:
                        std::cout << "Not enough stock: only " << inv.find(id)->quantity
                                  << " available. Nothing was removed.\n";
                        break;
                    case Overflow:
                        std::cout << "Quantity too large; stock would overflow.\n";
                        break;
                    default:
                        break;
                }
                break;
            }
            case 6:
                inv.list();
                break;
            case 7:
                inv.showHistory();
                break;
            default:
                std::cout << "Invalid choice. Enter a number from the menu.\n";
        }
    }

    inv.save();
    std::cout << "Inventory saved. Goodbye!\n";
    return 0;
}
