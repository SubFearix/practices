#include <iostream>
#include "database.h"
#include "table.h"
#include "parsing.h"
using namespace std;
void printHelp() {
    cout << "\n=== Команды SQL базы данных ===" << endl;
    cout << "Доступные команды:" << endl;
    cout << "  SELECT <столбцы> FROM <таблицы> [WHERE <условие>]" << endl;
    cout << "  INSERT INTO <таблица> VALUES (<значения>)" << endl;
    cout << "  DELETE FROM <таблица> [WHERE <условие>]" << endl;
    cout << "  HELP - показать эту справку" << endl;
    cout << "  EXIT - выход из программы" << endl;
}

int main()
{
    try {
        cout << "Загрузка базы данных..." << endl;
        Database db;
        cout << "База данных успешно загружена!" << endl;

        printHelp();

        string sql;
        while (true) {
            cout << ">> ";
            getline(cin, sql);
            if (sql.empty()) {
                continue;
            }
            if (!sql.empty() && sql.back() == ';') {
                sql.pop_back();
            }

            if (sql == "EXIT" || sql == "exit") {
                cout << "Выход из программы..." << endl;
                break;
            }
            if (sql == "HELP" || sql == "help") {
                printHelp();
                continue;
            }
            try {
                cout << "Выполнение: " << sql << endl;
                db.executeSQL(sql);
            }
            catch (const exception& e) {
                cerr << "Ошибка: " << e.what() << endl;
            }
            cout << endl;
        }

    }
    catch (const exception& e) {
        cerr << "Критическая ошибка при загрузке базы данных: " << e.what() << endl;
        cerr << "Убедитесь, что файл schema.json существует и имеет правильный формат." << endl;
        return 1;
    }
    return 0;
}