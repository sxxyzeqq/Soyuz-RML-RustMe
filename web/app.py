import os
import sys
import json
import uuid
import secrets
import string
import hashlib
import logging
from datetime import datetime, timedelta
from functools import wraps
from dotenv import load_dotenv
from flask import Flask, render_template, request, redirect, url_for, session, flash, jsonify, send_file
import sqlite3
import requests

load_dotenv()

DB_PATH = os.getenv("DB_PATH", "../backup/bot/data/users.db")
SECRET_KEY = os.getenv("SECRET_KEY", secrets.token_hex(32))
ADMIN_LOGIN = os.getenv("ADMIN_LOGIN", "midenn")
ADMIN_PASSWORD_HASH = os.getenv("ADMIN_PASSWORD_HASH", hashlib.sha256("admin123".encode()).hexdigest())
TG_SUPPORT = os.getenv("TG_SUPPORT", "qwwqwqwqwqwsdad")
TURNSTILE_SITE_KEY = os.getenv("TURNSTILE_SITE_KEY", "")
TURNSTILE_SECRET_KEY = os.getenv("TURNSTILE_SECRET_KEY", "")

PRODUCT_NAME = "$oyuz | Astrea"
LOADER_FILE_PATH = os.getenv("LOADER_FILE_PATH", "../files/system32.exe")

SUBSCRIPTION_PLANS = {
    "7d": {"title": "7 дней", "days": 7, "price": 100, "price_display": "100 ₽", "emoji": "🔥", "badge": "7 DAYS"},
    "1m": {"title": "1 месяц", "days": 30, "price": 350, "price_display": "350 ₽", "emoji": "💎", "badge": "1 MONTH"},
    "forever": {"title": "Навсегда", "days": None, "price": 650, "price_display": "650 ₽", "emoji": "👑", "badge": "LIFETIME"},
}

app = Flask(__name__)
app.secret_key = SECRET_KEY
app.config['MAX_CONTENT_LENGTH'] = 50 * 1024 * 1024

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    os.makedirs(os.path.dirname(DB_PATH) if os.path.dirname(DB_PATH) else '.', exist_ok=True)
    conn = get_db()
    c = conn.cursor()
    c.execute("""CREATE TABLE IF NOT EXISTS accounts (
        id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, username TEXT DEFAULT '',
        login TEXT UNIQUE, password_hash TEXT, hwid TEXT DEFAULT '', is_paid INTEGER DEFAULT 0,
        payment_date TEXT, subscription_expires TEXT, registered_at TEXT)""")
    c.execute("""CREATE TABLE IF NOT EXISTS payments (
        id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, account_id INTEGER,
        invoice_id TEXT, amount REAL, currency TEXT, status TEXT, plan_id TEXT DEFAULT '1m',
        created_at TEXT, payment_method TEXT DEFAULT 'telegram')""")
    c.execute("""CREATE TABLE IF NOT EXISTS subscription_keys (
        id INTEGER PRIMARY KEY AUTOINCREMENT, key_value TEXT UNIQUE, plan_id TEXT,
        is_used INTEGER DEFAULT 0, used_by INTEGER, used_at TEXT, created_by INTEGER, created_at TEXT)""")
    try: c.execute("ALTER TABLE accounts ADD COLUMN username TEXT DEFAULT ''")
    except: pass
    try: c.execute("ALTER TABLE payments ADD COLUMN plan_id TEXT DEFAULT '1m'")
    except: pass
    try: c.execute("ALTER TABLE payments ADD COLUMN payment_method TEXT DEFAULT 'telegram'")
    except: pass
    try: c.execute("ALTER TABLE payments ADD COLUMN account_id INTEGER")
    except: pass
    conn.commit()
    conn.close()

def hash_password(password: str) -> str:
    return hashlib.sha256(password.encode()).hexdigest()

def verify_turnstile(token: str) -> bool:
    if not TURNSTILE_SECRET_KEY:
        return True
    try:
        resp = requests.post(
            'https://challenges.cloudflare.com/turnstile/v0/siteverify',
            data={'secret': TURNSTILE_SECRET_KEY, 'response': token}
        )
        return resp.json().get('success', False)
    except:
        return False

def calculate_subscription_expires(current_expires, days):
    if days is None: return "Навсегда"
    now = datetime.now()
    base = now
    if current_expires and current_expires != "Навсегда":
        try:
            parsed = datetime.fromisoformat(current_expires)
            if parsed > now: base = parsed
        except ValueError: pass
    return (base + timedelta(days=days)).strftime("%Y-%m-%d %H:%M:%S")

def login_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if 'user_id' not in session:
            flash('Войдите в аккаунт', 'error')
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated

def admin_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if not session.get('admin'):
            flash('Доступ запрещён', 'error')
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated

@app.route('/')
def index():
    if 'user_id' in session: return redirect(url_for('dashboard'))
    return render_template('index.html', product_name=PRODUCT_NAME)

@app.route('/register', methods=['GET', 'POST'])
def register():
    if 'user_id' in session: return redirect(url_for('dashboard'))
    if request.method == 'POST':
        if not verify_turnstile(request.form.get('cf-turnstile-response', '')):
            flash('Пройдите проверку капчи', 'error')
            return render_template('register.html', site_key=TURNSTILE_SITE_KEY)
        login = request.form.get('login', '').strip()
        password = request.form.get('password', '').strip()
        if len(login) < 4:
            flash('Логин слишком короткий (мин. 4 символа)', 'error')
            return render_template('register.html', site_key=TURNSTILE_SITE_KEY)
        if not all(c.isalnum() or c == '_' for c in login):
            flash('Логин: только a-z, 0-9, _', 'error')
            return render_template('register.html', site_key=TURNSTILE_SITE_KEY)
        if len(password) < 6:
            flash('Пароль слишком короткий (мин. 6 символов)', 'error')
            return render_template('register.html', site_key=TURNSTILE_SITE_KEY)
        conn = get_db()
        existing = conn.execute("SELECT id FROM accounts WHERE login = ?", (login,)).fetchone()
        if existing:
            flash('Логин занят', 'error')
            conn.close()
            return render_template('register.html', site_key=TURNSTILE_SITE_KEY)
        pw_hash = hash_password(password)
        now = datetime.now().isoformat()
        conn.execute("INSERT INTO accounts (user_id, username, login, password_hash, registered_at) VALUES (?, ?, ?, ?, ?)", (None, '', login, pw_hash, now))
        conn.commit()
        account = conn.execute("SELECT * FROM accounts WHERE login = ?", (login,)).fetchone()
        conn.close()
        session['user_id'] = account['id']
        session['login'] = account['login']
        if account['login'] == 'midenn': session['admin'] = True
        flash('Аккаунт создан!', 'success')
        return redirect(url_for('dashboard'))
    return render_template('register.html', site_key=TURNSTILE_SITE_KEY)

@app.route('/login', methods=['GET', 'POST'])
def login():
    if 'user_id' in session: return redirect(url_for('dashboard'))
    if request.method == 'POST':
        if not verify_turnstile(request.form.get('cf-turnstile-response', '')):
            flash('Пройдите проверку капчи', 'error')
            return render_template('login.html', site_key=TURNSTILE_SITE_KEY)
        login = request.form.get('login', '').strip()
        password = request.form.get('password', '').strip()
        if login == ADMIN_LOGIN and hash_password(password) == ADMIN_PASSWORD_HASH:
            session['admin'] = True
            session['user_id'] = 0
            session['login'] = 'admin'
            return redirect(url_for('admin'))
        conn = get_db()
        account = conn.execute("SELECT * FROM accounts WHERE login = ?", (login,)).fetchone()
        conn.close()
        if not account or account['password_hash'] != hash_password(password):
            flash('Неверный логин или пароль', 'error')
            return render_template('login.html', site_key=TURNSTILE_SITE_KEY)
        session['user_id'] = account['id']
        session['login'] = account['login']
        if account['login'] == 'midenn': session['admin'] = True
        flash('Вход выполнен', 'success')
        return redirect(url_for('dashboard'))
    return render_template('login.html', site_key=TURNSTILE_SITE_KEY)

@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('index'))

@app.route('/dashboard')
@login_required
def dashboard():
    conn = get_db()
    account = conn.execute("SELECT * FROM accounts WHERE id = ?", (session['user_id'],)).fetchone()
    conn.close()
    if not account:
        session.clear()
        return redirect(url_for('login'))
    return render_template('dashboard.html', account=account, product_name=PRODUCT_NAME)

@app.route('/payment')
@login_required
def payment():
    conn = get_db()
    account = conn.execute("SELECT * FROM accounts WHERE id = ?", (session['user_id'],)).fetchone()
    conn.close()
    return render_template('payment.html', account=account, plans=SUBSCRIPTION_PLANS, product_name=PRODUCT_NAME, tg_support=TG_SUPPORT)

@app.route('/download')
@login_required
def download():
    conn = get_db()
    account = conn.execute("SELECT * FROM accounts WHERE id = ?", (session['user_id'],)).fetchone()
    conn.close()
    if not account or not account['is_paid']:
        flash('Нет активной подписки', 'error')
        return redirect(url_for('payment'))
    if not os.path.exists(LOADER_FILE_PATH):
        flash('Файл недоступен', 'error')
        return redirect(url_for('dashboard'))
    filename = request.args.get('name', 'loader')
    filename = "".join(c for c in filename if c.isalnum() or c in "._- ")
    filename = filename.strip()
    if not filename.endswith('.exe'): filename += '.exe'
    return send_file(LOADER_FILE_PATH, as_attachment=True, download_name=filename)

@app.route('/profile')
@login_required
def profile():
    conn = get_db()
    account = conn.execute("SELECT * FROM accounts WHERE id = ?", (session['user_id'],)).fetchone()
    conn.close()
    return render_template('profile.html', account=account, tg_support=TG_SUPPORT)

@app.route('/profile/change-password', methods=['POST'])
@login_required
def change_password():
    new_password = request.form.get('password', '').strip()
    if len(new_password) < 6:
        flash('Пароль слишком короткий (мин. 6 символов)', 'error')
        return redirect(url_for('profile'))
    conn = get_db()
    conn.execute("UPDATE accounts SET password_hash = ? WHERE id = ?", (hash_password(new_password), session['user_id']))
    conn.commit()
    conn.close()
    flash('Пароль изменён', 'success')
    return redirect(url_for('profile'))

@app.route('/profile/reset-hwid')
@login_required
def profile_reset_hwid_page():
    return render_template('reset_hwid.html', product_name=PRODUCT_NAME, tg_support=TG_SUPPORT)

@app.route('/admin')
@admin_required
def admin():
    conn = get_db()
    users = conn.execute("SELECT * FROM accounts ORDER BY id DESC").fetchall()
    stats = {
        'total': conn.execute("SELECT COUNT(*) FROM accounts").fetchone()[0],
        'paid': conn.execute("SELECT COUNT(*) FROM accounts WHERE is_paid=1").fetchone()[0],
        'revenue': conn.execute("SELECT COALESCE(SUM(amount),0) FROM payments WHERE status='paid'").fetchone()[0],
    }
    conn.close()
    return render_template('admin.html', users=users, stats=stats)

@app.route('/admin/grant', methods=['POST'])
@admin_required
def admin_grant():
    user_id = request.form.get('user_id')
    days = request.form.get('days')
    try: days = int(days)
    except:
        flash('Некорректное количество дней', 'error')
        return redirect(url_for('admin'))
    conn = get_db()
    account = conn.execute("SELECT * FROM accounts WHERE id = ?", (user_id,)).fetchone()
    if account:
        expires = calculate_subscription_expires(account['subscription_expires'], days)
        conn.execute("UPDATE accounts SET is_paid = 1, payment_date = ?, subscription_expires = ? WHERE id = ?", (datetime.now().isoformat(), expires, user_id))
        conn.commit()
        flash(f'Подписка выдана: {account["login"]} на {days} дней', 'success')
    conn.close()
    return redirect(url_for('admin'))

@app.route('/admin/reset-hwid', methods=['POST'])
@admin_required
def admin_reset_hwid():
    user_id = request.form.get('user_id')
    conn = get_db()
    conn.execute("UPDATE accounts SET hwid = '' WHERE id = ?", (user_id,))
    conn.commit()
    conn.close()
    flash('HWID сброшен', 'success')
    return redirect(url_for('admin'))

@app.route('/admin/reset-hwid-all', methods=['POST'])
@admin_required
def admin_reset_hwid_all():
    conn = get_db()
    conn.execute("UPDATE accounts SET hwid = ''")
    conn.commit()
    conn.close()
    flash('HWID сброшен всем пользователям', 'success')
    return redirect(url_for('admin'))

@app.route('/admin/grant-all', methods=['POST'])
@admin_required
def admin_grant_all():
    days = request.form.get('days')
    try: days = int(days)
    except:
        flash('Некорректное количество дней', 'error')
        return redirect(url_for('admin'))
    now = datetime.now()
    conn = get_db()
    users = conn.execute("SELECT * FROM accounts").fetchall()
    for user in users:
        expires = calculate_subscription_expires(user['subscription_expires'], days)
        conn.execute("UPDATE accounts SET is_paid = 1, payment_date = ?, subscription_expires = ? WHERE id = ?", (now.isoformat(), expires, user['id']))
    conn.commit()
    conn.close()
    flash(f'Подписка выдана всем пользователям на {days} дней', 'success')
    return redirect(url_for('admin'))

@app.route('/admin/revoke-subscription', methods=['POST'])
@admin_required
def admin_revoke_subscription():
    user_id = request.form.get('user_id')
    conn = get_db()
    conn.execute("UPDATE accounts SET is_paid = 0, subscription_expires = NULL, payment_date = NULL WHERE id = ?", (user_id,))
    conn.commit()
    conn.close()
    flash('Подписка снята', 'success')
    return redirect(url_for('admin'))

@app.route('/admin/revoke-subscription-multi', methods=['POST'])
@admin_required
def admin_revoke_subscription_multi():
    raw = request.form.get('user_ids', '').strip()
    if not raw:
        flash('Не указаны ID пользователей', 'error')
        return redirect(url_for('admin'))
    ids = []
    for part in raw.replace(',', ' ').split():
        part = part.strip()
        if part.isdigit(): ids.append(int(part))
    if not ids:
        flash('Некорректные ID пользователей', 'error')
        return redirect(url_for('admin'))
    conn = get_db()
    placeholders = ','.join('?' for _ in ids)
    conn.execute(f"UPDATE accounts SET is_paid = 0, subscription_expires = NULL, payment_date = NULL WHERE id IN ({placeholders})", ids)
    conn.commit()
    conn.close()
    flash(f'Подписка снята у {len(ids)} пользователей', 'success')
    return redirect(url_for('admin'))

@app.route('/api/login', methods=['POST'])
def api_login():
    data = request.get_json()
    login = data.get('login')
    password = data.get('password')
    hwid = data.get('hwid', '')
    if not login or not password:
        return jsonify({"error": "Login and password required"}), 400
    conn = get_db()
    account = conn.execute("SELECT * FROM accounts WHERE login = ?", (login,)).fetchone()
    if not account or account['password_hash'] != hash_password(password):
        conn.close()
        return jsonify({"error": "Invalid credentials"}), 401
    if not account['is_paid']:
        conn.close()
        return jsonify({"error": "No active subscription"}), 403
    if not account['hwid']:
        conn.execute("UPDATE accounts SET hwid = ? WHERE id = ?", (hwid, account['id']))
        conn.commit()
    elif account['hwid'] != hwid:
        conn.close()
        return jsonify({"error": "HWID mismatch"}), 403
    result = {"status": "ok", "login": account['login'], "expires": account['subscription_expires']}
    conn.close()
    return jsonify(result)

if __name__ == '__main__':
    init_db()
    app.run(host='0.0.0.0', port=5000, debug=True)
PYEOF