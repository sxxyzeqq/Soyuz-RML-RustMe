import sqlite3
import hashlib
import os
from datetime import datetime, timedelta
from config import Config

def get_db():
    db_path = Config.DATABASE
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn

def init_db():
    conn = get_db()
    cursor = conn.cursor()
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT UNIQUE,
            hwid TEXT,
            subscription_type TEXT DEFAULT NULL,
            subscription_start TEXT,
            subscription_end TEXT,
            is_active INTEGER DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            last_login TEXT
        )
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS payments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            amount REAL NOT NULL,
            currency TEXT DEFAULT 'RUB',
            plan TEXT NOT NULL,
            status TEXT DEFAULT 'pending',
            payment_id TEXT UNIQUE,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            completed_at TEXT,
            FOREIGN KEY (user_id) REFERENCES users (id)
        )
    ''')
    
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS hwid_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            hwid TEXT NOT NULL,
            action TEXT NOT NULL,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users (id)
        )
    ''')
    
    conn.commit()
    conn.close()

def hash_password(password):
    return hashlib.sha256(password.encode('utf-8')).hexdigest()

def create_user(username, password, email=None):
    conn = get_db()
    cursor = conn.cursor()
    try:
        password_hash = hash_password(password)
        cursor.execute(
            'INSERT INTO users (username, password_hash, email) VALUES (?, ?, ?)',
            (username, password_hash, email)
        )
        conn.commit()
        return cursor.lastrowid
    except sqlite3.IntegrityError:
        return None
    finally:
        conn.close()

def verify_user(username, password):
    conn = get_db()
    cursor = conn.cursor()
    password_hash = hash_password(password)
    cursor.execute(
        'SELECT * FROM users WHERE username = ? AND password_hash = ?',
        (username, password_hash)
    )
    user = cursor.fetchone()
    if user:
        cursor.execute(
            'UPDATE users SET last_login = ? WHERE id = ?',
            (datetime.now().isoformat(), user['id'])
        )
        conn.commit()
    conn.close()
    return user

def get_user_by_id(user_id):
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM users WHERE id = ?', (user_id,))
    user = cursor.fetchone()
    conn.close()
    return user

def get_user_by_username(username):
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM users WHERE username = ?', (username,))
    user = cursor.fetchone()
    conn.close()
    return user

def update_subscription(user_id, plan, duration_days):
    conn = get_db()
    cursor = conn.cursor()
    now = datetime.now()
    start = now.isoformat()
    end = (now + timedelta(days=duration_days)).isoformat()
    
    cursor.execute(
        '''UPDATE users 
           SET subscription_type = ?, subscription_start = ?, 
               subscription_end = ?, is_active = 1 
           WHERE id = ?''',
        (plan, start, end, user_id)
    )
    conn.commit()
    conn.close()

def bind_hwid(user_id, hwid):
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute('UPDATE users SET hwid = ? WHERE id = ?', (hwid, user_id))
    cursor.execute(
        'INSERT INTO hwid_logs (user_id, hwid, action) VALUES (?, ?, ?)',
        (user_id, hwid, 'bound')
    )
    conn.commit()
    conn.close()

def reset_hwid(user_id):
    conn = get_db()
    cursor = conn.cursor()
    old_hwid = get_user_by_id(user_id)['hwid']
    cursor.execute('UPDATE users SET hwid = NULL WHERE id = ?', (user_id,))
    cursor.execute(
        'INSERT INTO hwid_logs (user_id, hwid, action) VALUES (?, ?, ?)',
        (user_id, old_hwid, 'reset')
    )
    conn.commit()
    conn.close()

def create_payment(user_id, amount, plan, payment_id):
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute(
        'INSERT INTO payments (user_id, amount, plan, payment_id) VALUES (?, ?, ?, ?)',
        (user_id, amount, plan, payment_id)
    )
    conn.commit()
    payment_id_row = cursor.lastrowid
    conn.close()
    return payment_id_row

def update_payment_status(payment_id, status):
    conn = get_db()
    cursor = conn.cursor()
    now = datetime.now().isoformat() if status == 'completed' else None
    cursor.execute(
        'UPDATE payments SET status = ?, completed_at = ? WHERE payment_id = ?',
        (status, now, payment_id)
    )
    conn.commit()
    conn.close()

def get_user_payments(user_id):
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute(
        'SELECT * FROM payments WHERE user_id = ? ORDER BY created_at DESC',
        (user_id,)
    )
    payments = cursor.fetchall()
    conn.close()
    return payments

def get_all_users():
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM users ORDER BY created_at DESC')
    users = cursor.fetchall()
    conn.close()
    return users

def get_all_payments():
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM payments ORDER BY created_at DESC')
    payments = cursor.fetchall()
    conn.close()
    return payments

def get_stats():
    conn = get_db()
    cursor = conn.cursor()
    
    cursor.execute('SELECT COUNT(*) FROM users')
    total_users = cursor.fetchone()[0]
    
    cursor.execute('SELECT COUNT(*) FROM users WHERE is_active = 1')
    active_subscribers = cursor.fetchone()[0]
    
    cursor.execute('SELECT COUNT(*) FROM payments WHERE status = "completed"')
    completed_payments = cursor.fetchone()[0]
    
    cursor.execute('SELECT SUM(amount) FROM payments WHERE status = "completed"')
    total_revenue = cursor.fetchone()[0] or 0
    
    cursor.execute('''
        SELECT subscription_type, COUNT(*) as count 
        FROM users 
        WHERE subscription_type IS NOT NULL 
        GROUP BY subscription_type
    ''')
    plan_distribution = dict(cursor.fetchall())
    
    conn.close()
    
    return {
        'total_users': total_users,
        'active_subscribers': active_subscribers,
        'completed_payments': completed_payments,
        'total_revenue': total_revenue,
        'plan_distribution': plan_distribution
    }

def delete_user(user_id):
    conn = get_db()
    cursor = conn.cursor()
    cursor.execute('DELETE FROM payments WHERE user_id = ?', (user_id,))
    cursor.execute('DELETE FROM hwid_logs WHERE user_id = ?', (user_id,))
    cursor.execute('DELETE FROM users WHERE id = ?', (user_id,))
    conn.commit()
    conn.close()
