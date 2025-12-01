#include "database.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "vector.h"
#include <vector>

using json = nlohmann::json;
using namespace std;

void Database::loadSchema()
{
    ifstream file("schema.json");
    json data = json::parse(file);

    name = data["name"];
    tuplesLimit = data["tuples_limit"];
    json structure = data["structure"];
    directory = name;

    for (const auto& table: structure.items())
    {
        string tableName = table.key();
        Vector columns = table.value().get<vector<string>>();
        Table* tableObj = new Table(tableName, columns, directory, tuplesLimit);
        tables.addElement(tableName, tableObj);
    }
    file.close();
}

Table* Database::getTable(const string& tableName) const
{
    Table* table = tables.findElement(tableName);
    if (table == nullptr)
    {
        throw runtime_error("Таблица '" + tableName + "' не найдена");
    }
    return table;
}

void Database::executeInsert(const SQLQuery& query) {
    Table* table = getTable(query.insertTable);
    table->insertData(query.insertValues);
    cout << "Данные успешно вставлены в таблицу '" << query.insertTable << "'" << endl;
}

void Database::executeDelete(const SQLQuery& query) {
    Table* table = getTable(query.deleteTable);
    table->deleteData(query.deleteConditions);
    if (table->selectAll().size() == 1) {
        table->resetPK();
    }
    cout << "Данные удалены из таблицы '" << query.deleteTable << "'" << endl;
}

int getColIndex(const Vector<string>& headers, const string& colName) {
    for (int i = 0; i < headers.size(); i++) {
        if (headers[i] == colName) return i;
    }
    return -1;
}

bool Database::checkConditionJoined(const Condition& condition, const Vector<string>& headers, const Vector<string>& row)
{
    if (condition.getSign() == "AND")
    {
        if (!condition.getLeft() || !condition.getRight()) {
            return false;
        }

        return checkConditionJoined(*condition.getLeft(), headers, row) &&
               checkConditionJoined(*condition.getRight(), headers, row);
    }

    if (condition.getSign() == "OR")
    {
        if (!condition.getLeft() && !condition.getRight()) {
            return false;
        }
        return checkConditionJoined(*condition.getLeft(), headers, row) ||
               checkConditionJoined(*condition.getRight(), headers, row);
    }

    if (condition.getSign() == "=")
    {
        const int leftIdx = getColIndex(headers, condition.getName());
        string rightVal = condition.getValue();
        const int rightIdx = getColIndex(headers, condition.getValue());
        const string leftVal = (leftIdx != -1 && leftIdx < row.size()) ? row[leftIdx] : "";
        if (rightIdx != -1 && rightIdx < row.size()) {
            rightVal = row[rightIdx];
        }
        return leftVal == rightVal;
    }
    return false;
}

bool Database::checkWhereJoined(const Vector<Condition*>& conditions, const Vector<string>& headers, const Vector<string>& row)
{
    if (conditions.empty()) return true;
    for (const Condition* cond: conditions) {
        if (!checkConditionJoined(*cond, headers, row)) return false;
    }
    return true;
}

void Database::executeJoinRecursive(
    const SQLQuery& query,
    size_t tableIndex,
    const Vector<string>& currentHeaders,
    const Vector<string>& currentRow,
    Vector<Vector<string>>& result)
{
    if (tableIndex >= query.fromTables.size()) {
        if (checkWhereJoined(query.whereConditions, currentHeaders, currentRow)) {
            Vector<string> filteredRow;
            for (const string& colName: query.selectColumns) {
                int colIdx = getColIndex(currentHeaders, colName);
                if (colIdx != -1 && colIdx < currentRow.size()) {
                    filteredRow.push_back(currentRow[colIdx]);
                }
            }
            result.push_back(filteredRow);
        }
        return;
    }

    Table* currentTable = getTable(query.fromTables[tableIndex]);
    Vector<string> tableColumns = currentTable->getColumns();

    Vector<string> newHeaders = currentHeaders;
    for (const string& col: tableColumns) {
        newHeaders.push_back(query.fromTables[tableIndex] + "." + col);
    }

    string tablePath = currentTable->getPath();
    int fileIndex = 1;

    while (true) {
        string filePath = tablePath + "/" + to_string(fileIndex) + ".csv";
        ifstream file(filePath);

        if (!file.is_open()) {
            break;
        }

        string line;
        getline(file, line);

        while (getline(file, line)) {
            Vector<string> tableRow = Table::splitLine(line);
            if (!tableRow.empty()) {
                tableRow.erase(tableRow.begin());
            }
            Vector<string> newRow = currentRow;
            newRow.insert(newRow.end(), tableRow.begin(), tableRow.end());

            executeJoinRecursive(query, tableIndex + 1, newHeaders, newRow, result);
        }
        file.close();
        fileIndex++;
    }
}

Vector<Vector<string>> Database::executeJoin(const SQLQuery& query)
{
    if (query.fromTables.empty()) {
        throw runtime_error("JOIN требует как минимум одну таблицу");
    }
    Vector<Vector<string>> result;

    Vector<string> allHeaders;
    for (const string& tableName: query.fromTables) {
        Table* table = getTable(tableName);
        Vector<string> columns = table->getColumns();
        for (const string& col: columns) {
            allHeaders.push_back(tableName + "." + col);
        }
    }

    Vector<string> selectedHeaders;
    for (const string& colName: query.selectColumns) {
        selectedHeaders.push_back(colName);
    }
    result.push_back(selectedHeaders);

    const Vector<string> emptyRow;
    const Vector<string> emptyHeaders;
    executeJoinRecursive(query, 0, emptyHeaders, emptyRow, result);
    return result;
}

void Database::executeSelect(const SQLQuery& query) {
    if (query.fromTables.empty()) {
        throw runtime_error("SELECT запрос должен содержать хотя бы одну таблицу в FROM");
    }

    if (query.fromTables.size() == 1) {
        Table* table = getTable(query.fromTables[0]);
        Vector<Vector<string>> result = table->findData(query.selectColumns, query.whereConditions);
        printResult(result);
    }
    else {
        Vector<Vector<string>> result = executeJoin(query);
        printResult(result);
    }
}

void Database::executeSQL(const string& sql)
{
    SQLParser parser;
    SQLQuery query = parser.parse(sql);
    try
    {
        switch(query.type) {
        case SQLQuery::SELECT:
            executeSelect(query);
            break;
        case SQLQuery::INSERT:
            executeInsert(query);
            break;
        case SQLQuery::DELETE:
            executeDelete(query);
            break;
        default:
            throw runtime_error("Неизвестный тип SQL запроса");
        }
    } catch(const exception& e)
    {
        cerr << e.what() << endl;
    }
}

void Database::printResult(const Vector<Vector<string>>& result) {
    if (result.empty()) {
        cout << "Пустой результат" << endl;
        return;
    }
    for (const auto& row : result) {
        for (const auto& cell : row) {
            cout << cell << " | ";
        }
        cout << endl;
    }
    cout << "Всего строк: " << (result.size() - 1) << endl;
}