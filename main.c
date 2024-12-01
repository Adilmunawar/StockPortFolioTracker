#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>

#define API_KEY "947NQI371MIH4ODK"

// SQLite Database Setup
int create_db() {
    sqlite3 *db;
    char *err_msg = 0;

    int rc = sqlite3_open("portfolio.db", &db);
    if (rc) {
        printf("Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS portfolio ("
                      "ticker TEXT PRIMARY KEY, "
                      "shares INTEGER, "
                      "price REAL, "
                      "total_value REAL);";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}

// HTTP Request - Fetch Stock Price Using libcurl
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
    strcat(data, ptr);
    return size * nmemb;
}

float fetch_stock_price(const char *ticker) {
    CURL *curl;
    CURLcode res;
    char url[256];
    char response[1024] = "";

    snprintf(url, sizeof(url), "https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=%s&interval=5min&apikey=%s", ticker, API_KEY);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return -1;
        }

        // Parse the JSON response here to extract the latest stock price
        // For simplicity, we'll just return a dummy price here
        float price = 100.0f;  // Normally, you would parse the response to get the price

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return price;
}

// Add Stock to Database
int add_stock_to_db(const char *ticker, int shares, float price) {
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open("portfolio.db", &db);
    if (rc) {
        printf("Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    float total_value = price * shares;
    char sql[512];
    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO portfolio (ticker, shares, price, total_value) VALUES ('%s', %d, %f, %f);", ticker, shares, price, total_value);

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}

// Real-time Updates in a Separate Thread
void *real_time_updates(void *arg) {
    while (1) {
        // Retrieve all stocks from the database and update their prices
        sqlite3 *db;
        sqlite3_stmt *stmt;
        sqlite3_open("portfolio.db", &db);
        const char *sql = "SELECT ticker FROM portfolio";
        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *ticker = (const char*)sqlite3_column_text(stmt, 0);
            float price = fetch_stock_price(ticker);
            if (price != -1) {
                add_stock_to_db(ticker, 0, price);  // Update the price
            }
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        sleep(300);  // Fetch prices every 5 minutes
    }
    return NULL;
}

// View Portfolio
void view_portfolio() {
    sqlite3 *db;
    sqlite3_open("portfolio.db", &db);
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM portfolio";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *ticker = (const char*)sqlite3_column_text(stmt, 0);
        int shares = sqlite3_column_int(stmt, 1);
        float price = sqlite3_column_double(stmt, 2);
        float total_value = sqlite3_column_double(stmt, 3);
        printf("%s: %d shares at %.2f each, Total Value: %.2f\n", ticker, shares, price, total_value);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// Main Menu
void main_menu() {
    int choice;
    char ticker[10];
    int shares;
    printf("1. View Portfolio\n2. Add Stock\n3. Update Stock Price\n4. Exit\n");
    scanf("%d", &choice);

    switch(choice) {
        case 1:
            view_portfolio();
            break;
        case 2:
            printf("Enter Stock Ticker: ");
            scanf("%s", ticker);
            printf("Enter Number of Shares: ");
            scanf("%d", &shares);
            float price = fetch_stock_price(ticker);
            if (price != -1) {
                add_stock_to_db(ticker, shares, price);
                printf("Added %d shares of %s at $%.2f each.\n", shares, ticker, price);
            }
            break;
        case 3:
            printf("Enter Stock Ticker: ");
            scanf("%s", ticker);
            price = fetch_stock_price(ticker);
            if (price != -1) {
                add_stock_to_db(ticker, 0, price);  // Update price (0 shares to just update)
                printf("Updated price of %s to $%.2f.\n", ticker, price);
            }
            break;
        case 4:
            printf("Exiting...\n");
            exit(0);
        default:
            printf("Invalid choice.\n");
    }
}

int main() {
    create_db();  // Create database if it doesn't exist

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, real_time_updates, NULL);  // Start real-time updates in background

    while (1) {
        main_menu();
    }

    pthread_join(thread_id, NULL);  // Wait for the thread to finish (it won't)
    return 0;
}
