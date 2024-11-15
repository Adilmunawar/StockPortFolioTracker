import requests
import sqlite3
import tkinter as tk
from tkinter import messagebox
import threading
import time

# Alpha Vantage API Key
API_KEY = '947NQI371MIH4ODK'

# Database Setup (SQLite)
def create_db():
    conn = sqlite3.connect('portfolio.db')
    cursor = conn.cursor()
    cursor.execute('''CREATE TABLE IF NOT EXISTS portfolio (
                        ticker TEXT PRIMARY KEY,
                        shares INTEGER,
                        price REAL,
                        total_value REAL)''')
    conn.commit()
    conn.close()

def add_stock_to_db(ticker, shares, price):
    conn = sqlite3.connect('portfolio.db')
    cursor = conn.cursor()
    total_value = price * shares
    cursor.execute('''INSERT OR REPLACE INTO portfolio (ticker, shares, price, total_value)
                      VALUES (?, ?, ?, ?)''', (ticker, shares, price, total_value))
    conn.commit()
    conn.close()

def update_stock_in_db(ticker, price):
    conn = sqlite3.connect('portfolio.db')
    cursor = conn.cursor()
    cursor.execute('''UPDATE portfolio SET price = ?, total_value = price * shares WHERE ticker = ?''', (price, ticker))
    conn.commit()
    conn.close()

def get_all_stocks_from_db():
    conn = sqlite3.connect('portfolio.db')
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM portfolio')
    stocks = cursor.fetchall()
    conn.close()
    return stocks

# Fetch stock price from Alpha Vantage API
def fetch_stock_price(ticker):
    url = f'https://www.alphavantage.co/query'
    params = {
        'function': 'TIME_SERIES_INTRADAY',
        'symbol': ticker,
        'interval': '5min',  # Set interval to fetch stock price every 5 minutes
        'apikey': API_KEY
    }
    response = requests.get(url, params=params)
    
    if response.status_code == 200:
        data = response.json()
        try:
            latest_time = list(data['Time Series (5min)'].keys())[0]
            latest_close = data['Time Series (5min)'][latest_time]['4. close']
            return float(latest_close)
        except KeyError:
            print(f"Error: Could not retrieve stock data for {ticker}.")
            return None
    else:
        print("Error: Unable to fetch data from Alpha Vantage API.")
        return None

# Portfolio Class
class PortfolioApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Advanced Stock Portfolio Tracker")
        self.root.geometry("600x400")

        self.create_widgets()

    def create_widgets(self):
        self.stock_label = tk.Label(self.root, text="Enter Stock Ticker:")
        self.stock_label.pack(pady=10)

        self.ticker_entry = tk.Entry(self.root)
        self.ticker_entry.pack(pady=5)

        self.shares_label = tk.Label(self.root, text="Enter Number of Shares:")
        self.shares_label.pack(pady=10)

        self.shares_entry = tk.Entry(self.root)
        self.shares_entry.pack(pady=5)

        self.add_button = tk.Button(self.root, text="Add Stock", command=self.add_stock)
        self.add_button.pack(pady=10)

        self.update_button = tk.Button(self.root, text="Update Stock Price", command=self.update_stock)
        self.update_button.pack(pady=10)

        self.view_button = tk.Button(self.root, text="View Portfolio", command=self.view_portfolio)
        self.view_button.pack(pady=10)

        self.portfolio_frame = tk.Frame(self.root)
        self.portfolio_frame.pack(pady=20)

    def add_stock(self):
        ticker = self.ticker_entry.get().upper()
        shares = int(self.shares_entry.get())
        
        price = fetch_stock_price(ticker)
        if price is not None:
            add_stock_to_db(ticker, shares, price)
            messagebox.showinfo("Success", f"Added {shares} shares of {ticker} at ${price} each.")
            self.clear_entries()
        else:
            messagebox.showerror("Error", "Could not fetch the stock price.")

    def update_stock(self):
        ticker = self.ticker_entry.get().upper()
        price = fetch_stock_price(ticker)
        if price is not None:
            update_stock_in_db(ticker, price)
            messagebox.showinfo("Success", f"Updated {ticker} price to ${price}.")
            self.clear_entries()
        else:
            messagebox.showerror("Error", "Could not fetch the stock price.")

    def view_portfolio(self):
        self.clear_portfolio_frame()

        stocks = get_all_stocks_from_db()
        total_value = 0
        for stock in stocks:
            ticker, shares, price, total_value_stock = stock
            total_value += total_value_stock

            stock_label = tk.Label(self.portfolio_frame, text=f"{ticker}: {shares} shares at ${price} each")
            stock_label.pack()

        total_label = tk.Label(self.portfolio_frame, text=f"Total Portfolio Value: ${total_value:.2f}")
        total_label.pack()

    def clear_entries(self):
        self.ticker_entry.delete(0, tk.END)
        self.shares_entry.delete(0, tk.END)

    def clear_portfolio_frame(self):
        for widget in self.portfolio_frame.winfo_children():
            widget.destroy()

# Fetch stock prices in real-time using threading
def real_time_updates():
    while True:
        stocks = get_all_stocks_from_db()
        for stock in stocks:
            ticker, _, _, _ = stock
            price = fetch_stock_price(ticker)
            if price is not None:
                update_stock_in_db(ticker, price)
        time.sleep(300)  # Fetch prices every 5 minutes

# Run the main application
if __name__ == '__main__':
    create_db()

    root = tk.Tk()
    app = PortfolioApp(root)

    # Start real-time price updates in a separate thread
    threading.Thread(target=real_time_updates, daemon=True).start()

    root.mainloop()
  
