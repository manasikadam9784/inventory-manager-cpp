# Inventory Manager (C++)

A console-based inventory management system written in C++. It tracks items by ID, name, quantity and price, records stock-in and stock-out operations, and saves everything to CSV files so your data is still there the next time you run it.

## Features

- **Add items** with an ID, name, opening quantity and unit price
- **Search** by item ID or part of the name (case-insensitive)
- **Update** an item's name or price
- **Stock-in / stock-out** operations that adjust quantities
- **Negative stock prevention**: a stock-out larger than the quantity on hand is rejected and nothing changes
- **Transaction history**: every stock movement is logged with a timestamp and the resulting balance
- **CSV storage**: load your inventory from a CSV file (including one made in Excel) and save changes back automatically
- **Input validation**: invalid numbers, empty fields, duplicate IDs and unknown IDs are handled safely instead of crashing
- **Stock value report**: the item list shows the total value of all stock

## Requirements

- Any C++ compiler (GCC/MinGW, Clang, MSVC). The code works in C++98 and later, so no special settings are needed.
- Works on Windows, Linux and macOS.

## Build and Run

```bash
g++ -Wall -Wextra -o inventory inventory.cpp
./inventory          # on Windows: inventory.exe
```

If you use Dev-C++, open `inventory.cpp` and press **F11** (Compile & Run).

On startup the program asks for the path to your inventory CSV file:

```
Inventory CSV file path (press Enter to use inventory.csv):
```

Paste the full path (for example `C:\Users\you\Desktop\stock.csv`), or press Enter to use `inventory.csv` in the current folder. If the file does not exist, the program starts with an empty inventory and creates the file when it first saves.

## Menu

```
===== Inventory Manager =====
 1. Add an item
 2. Search items
 3. Update item (name / price)
 4. Stock-in (receive stock)
 5. Stock-out (remove stock)
 6. List all items
 7. View transaction history
 0. Save and exit
```

## Data Files

### Inventory file (CSV)

One item per row, with a header row:

```csv
id,name,quantity,price
1,Pen,100,10.00
2,"Notebook, A4",40,55.50
```

You can create this file in Excel: enter the four columns, then use **File → Save As → CSV (Comma delimited)**. Names that contain commas are quoted automatically. Close the file in Excel before running the program, otherwise Windows will block it from saving.

### Transaction log

`transactions.csv` is created in the program's folder. Each line records the time, item ID, type (`INITIAL`, `STOCK-IN` or `STOCK-OUT`), quantity and the balance afterwards:

```csv
2026-11-19 07:01:12,1,INITIAL,50,50
2026-11-19 07:01:12,1,STOCK-OUT,20,30
```

## Example Session

```
Choice: 5
Item ID: 1
Quantity removed: 60
Not enough stock: only 50 available. Nothing was removed.

Choice: 5
Item ID: 1
Quantity removed: 20
Stock removed. New quantity: 30
```

## How It Works

- Items are stored in a `std::map<int, Item>`, so they stay sorted by ID and lookups are fast.
- Operations such as `stockIn`, `stockOut` and `updateItem` return a result code instead of printing, which keeps the inventory logic separate from the menu code.
- Files are read and written with `std::ifstream` / `std::ofstream`. Malformed or duplicate lines in a CSV file are skipped with a warning.
- Quantities are checked for overflow, and prices must be zero or more.

## Project Structure

```
inventory-manager-cpp/
├── inventory.cpp      # complete source code
├── README.md
└── sample/
    └── inventory.csv  # optional example data
```

## Possible Improvements

- Low-stock alerts and reorder levels
- Delete items and edit quantities directly
- Categories, suppliers and barcodes
- Export reports (daily/weekly stock movement)
- Storing prices as integers (paise or cents) to avoid floating-point rounding
- Splitting the code into header and source files

## License

This project is released under the MIT License. Add a `LICENSE` file to your repository if you want to use it.

## Author

Your Name · [GitHub profile](https://github.com/your-username)
