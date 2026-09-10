import os
import asyncio
import secrets
import string
import hashlib
import logging
from datetime import datetime, timedelta
from dotenv import load_dotenv

load_dotenv()

from aiogram import Bot, Dispatcher, F
from aiogram.filters import Command, CommandStart
from aiogram.fsm.context import FSMContext
from aiogram.fsm.storage.memory import MemoryStorage
from aiogram.fsm.state import State, StatesGroup
from aiogram.types import InlineKeyboardMarkup, InlineKeyboardButton, CallbackQuery, Message, FSInputFile
from aiocryptopay import AioCryptoPay, Networks
import aiosqlite

# ======================== CONFIG ========================

BOT_TOKEN = os.getenv("BOT_TOKEN", "")
CRYPTO_PAY_API_KEY = os.getenv("CRYPTO_PAY_API_KEY", "")
ADMIN_ID = int(os.getenv("ADMIN_ID", "0"))

PRODUCT_NAME = "$oyuz|BETA"
LOADER_FILE_PATH = "files/loader.exe"
SUBSCRIPTION_PLANS = {
    "1d": {"title": "1 день", "days": 1, "price": 0.49, "emoji": "⚡", "badge": "Тест-драйв"},
    "7d": {"title": "7 дней", "days": 7, "price": 1.99, "emoji": "🔥", "badge": "Популярно"},
    "1m": {"title": "1 месяц", "days": 30, "price": 3.99, "emoji": "💎", "badge": "Выгодно"},
    "forever": {"title": "Навсегда", "days": None, "price": 8.99, "emoji": "👑", "badge": "Максимум"},
}

PRODUCT_DESCRIPTION = f"""
<b>⚔️ {PRODUCT_NAME}</b>
<i>Приватный чит-клиент для Minecraft RustMe</i>

╔════════════════════
║ ✨ <b>Возможности</b>
╠ AimBot с предиктом
╠ ESP / SoundESP / WallHack
╠ Chams / X-Ray / Night Mode
╠ Zoom / ViewModel Edit
╠ Auto Sprint / Time Scale
╚ Crosshair Overlay

🛡 <b>Защита:</b> привязка к 1 ПК по HWID
💳 <b>Оплата:</b> CryptoBot, USDT
"""

# ======================== UTILS ========================

def generate_login(length=8):
    chars = string.ascii_lowercase + string.digits
    return "oyuz_" + ''.join(secrets.choice(chars) for _ in range(length))

def generate_password(length=12):
    chars = string.ascii_letters + string.digits + "!@#$"
    return ''.join(secrets.choice(chars) for _ in range(length))

def hash_password(password: str) -> str:
    return hashlib.sha256(password.encode()).hexdigest()

def generate_subscription_key(length=20):
    chars = string.ascii_lowercase + string.digits
    return "ar_" + ''.join(secrets.choice(chars) for _ in range(length))

def calculate_subscription_expires(current_expires: str, days):
    if days is None:
        return "Навсегда"
    now = datetime.now()
    base = now
    if current_expires and current_expires != "Навсегда":
        try:
            parsed = datetime.fromisoformat(current_expires)
            if parsed > now:
                base = parsed
        except ValueError:
            pass
    return (base + timedelta(days=days)).strftime("%Y-%m-%d %H:%M:%S")

# ======================== CRYPTO PAY ========================

class CryptoPayService:
    def __init__(self, api_token: str):
        self.client = AioCryptoPay(token=api_token, network=Networks.MAIN_NET)

    async def create_invoice(self, amount: float, description: str, payload: str) -> dict:
        invoice = await self.client.create_invoice(
            asset="USDT", amount=amount, description=description,
            payload=payload, expires_in=1800
        )
        return {
            "invoice_id": invoice.invoice_id,
            "bot_url": invoice.bot_invoice_url,
            "mini_app_url": getattr(invoice, 'mini_app_invoice_url', None) or invoice.bot_invoice_url,
        }

    async def get_invoice_status(self, invoice_id: int) -> dict:
        invoices = await self.client.get_invoices(invoice_ids=[invoice_id])
        if invoices and len(invoices) > 0:
            inv = invoices[0]
            return {
                "invoice_id": inv.invoice_id, "status": inv.status,
                "amount": str(inv.amount), "currency": inv.asset,
                "payload": getattr(inv, 'payload', ''),
            }
        return {}

    async def close(self):
        await self.client.close()

# ======================== DATABASE ========================

DB_PATH = "data/users.db"

class Database:
    def __init__(self):
        self.db = None

    async def init(self):
        os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)
        self.db = await aiosqlite.connect(DB_PATH)
        await self.db.execute("""
            CREATE TABLE IF NOT EXISTS accounts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                login TEXT UNIQUE,
                password_hash TEXT,
                hwid TEXT DEFAULT '',
                is_paid INTEGER DEFAULT 0,
                payment_date TEXT,
                subscription_expires TEXT,
                registered_at TEXT
            )
        """)
        await self.db.execute("""
            CREATE TABLE IF NOT EXISTS payments (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER, account_id INTEGER,
                invoice_id INTEGER, amount REAL, currency TEXT,
                status TEXT, plan_id TEXT DEFAULT '1m', created_at TEXT
            )
        """)
        await self.db.execute("""
            CREATE TABLE IF NOT EXISTS subscription_keys (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                key_value TEXT UNIQUE,
                plan_id TEXT,
                is_used INTEGER DEFAULT 0,
                used_by INTEGER,
                used_at TEXT,
                created_by INTEGER,
                created_at TEXT
            )
        """)
        try:
            await self.db.execute("ALTER TABLE payments ADD COLUMN plan_id TEXT DEFAULT '1m'")
        except aiosqlite.OperationalError:
            pass
        await self.db.commit()

    async def get_account_by_user_id(self, user_id: int) -> dict:
        async with self.db.execute("SELECT * FROM accounts WHERE user_id = ?", (user_id,)) as cur:
            row = await cur.fetchone()
            if row:
                return dict(zip(["id","user_id","login","password_hash","hwid","is_paid","payment_date","subscription_expires","registered_at"], row))
            return None

    async def get_account_by_login(self, login: str) -> dict:
        async with self.db.execute("SELECT * FROM accounts WHERE login = ?", (login,)) as cur:
            row = await cur.fetchone()
            if row:
                return dict(zip(["id","user_id","login","password_hash","hwid","is_paid","payment_date","subscription_expires","registered_at"], row))
            return None

    async def create_account(self, user_id: int, login: str, password_hash: str) -> int:
        cur = await self.db.execute(
            "INSERT INTO accounts (user_id, login, password_hash, registered_at) VALUES (?, ?, ?, ?)",
            (user_id, login, password_hash, datetime.now().isoformat()))
        await self.db.commit()
        return cur.lastrowid

    async def set_paid(self, account_id: int, expires: str):
        await self.db.execute(
            "UPDATE accounts SET is_paid = 1, payment_date = ?, subscription_expires = ? WHERE id = ?",
            (datetime.now().isoformat(), expires, account_id))
        await self.db.commit()

    async def set_hwid(self, account_id: int, hwid: str):
        await self.db.execute("UPDATE accounts SET hwid = ? WHERE id = ?", (hwid, account_id))
        await self.db.commit()

    async def change_password(self, account_id: int, new_hash: str):
        await self.db.execute("UPDATE accounts SET password_hash = ? WHERE id = ?", (new_hash, account_id))
        await self.db.commit()

    async def add_payment(self, user_id, invoice_id, amount, currency, status, plan_id="1m"):
        await self.db.execute(
            "INSERT INTO payments (user_id, invoice_id, amount, currency, status, plan_id, created_at) VALUES (?,?,?,?,?,?,?)",
            (user_id, invoice_id, amount, currency, status, plan_id, datetime.now().isoformat()))
        await self.db.commit()

    async def get_pending_invoice(self, user_id: int):
        async with self.db.execute(
            "SELECT invoice_id, plan_id FROM payments WHERE user_id=? AND status='pending' ORDER BY id DESC LIMIT 1",
            (user_id,)) as cur:
            return await cur.fetchone()

    async def update_payment_status(self, invoice_id: int, status: str):
        await self.db.execute("UPDATE payments SET status = ? WHERE invoice_id = ?", (status, invoice_id))
        await self.db.commit()

    async def get_all_accounts(self) -> list:
        async with self.db.execute("SELECT * FROM accounts") as cur:
            rows = await cur.fetchall()
            return [dict(zip(["id","user_id","login","password_hash","hwid","is_paid","payment_date","subscription_expires","registered_at"], r)) for r in rows]

    async def get_stats(self) -> dict:
        async with self.db.execute("SELECT COUNT(*) FROM accounts") as cur:
            total = (await cur.fetchone())[0]
        async with self.db.execute("SELECT COUNT(*) FROM accounts WHERE is_paid=1") as cur:
            paid = (await cur.fetchone())[0]
        async with self.db.execute("SELECT COALESCE(SUM(amount),0) FROM payments WHERE status='paid'") as cur:
            revenue = (await cur.fetchone())[0]
        return {"total": total, "paid": paid, "revenue": revenue}

    async def delete_account(self, user_id: int) -> bool:
        async with self.db.execute("SELECT id FROM accounts WHERE user_id = ?", (user_id,)) as cur:
            row = await cur.fetchone()
            if not row:
                return False
        await self.db.execute("DELETE FROM payments WHERE user_id = ?", (user_id,))
        await self.db.execute("DELETE FROM accounts WHERE user_id = ?", (user_id,))
        await self.db.commit()
        return True

    async def create_subscription_key(self, plan_id: str, created_by: int) -> str:
        while True:
            key_value = generate_subscription_key()
            try:
                await self.db.execute(
                    "INSERT INTO subscription_keys (key_value, plan_id, created_by, created_at) VALUES (?, ?, ?, ?)",
                    (key_value, plan_id, created_by, datetime.now().isoformat()))
                await self.db.commit()
                return key_value
            except aiosqlite.IntegrityError:
                continue

    async def get_subscription_key(self, key_value: str) -> dict:
        async with self.db.execute("SELECT * FROM subscription_keys WHERE key_value = ?", (key_value,)) as cur:
            row = await cur.fetchone()
            if row:
                return dict(zip(["id","key_value","plan_id","is_used","used_by","used_at","created_by","created_at"], row))
            return None

    async def use_subscription_key(self, key_id: int, user_id: int):
        await self.db.execute(
            "UPDATE subscription_keys SET is_used = 1, used_by = ?, used_at = ? WHERE id = ?",
            (user_id, datetime.now().isoformat(), key_id))
        await self.db.commit()

    async def close(self):
        if self.db: await self.db.close()

# ======================== FSM ========================

class RegisterState(StatesGroup):
    waiting_for_login = State()
    waiting_for_password = State()

class KeyState(StatesGroup):
    waiting_for_key = State()

class AdminKeyState(StatesGroup):
    waiting_for_count = State()

# ======================== BOT ========================

class OyuzBot:
    def __init__(self):
        self.bot = Bot(token=BOT_TOKEN)
        storage = MemoryStorage()
        self.dp = Dispatcher(storage=storage)
        self.crypto = CryptoPayService(CRYPTO_PAY_API_KEY)
        self.db = Database()
        self._handlers()

    def _handlers(self):
        self.dp.message.register(self._start, CommandStart())
        self.dp.message.register(self._register, Command("register"))
        self.dp.message.register(self._login, Command("login"))
        self.dp.message.register(self._profile, Command("profile"))
        self.dp.message.register(self._help, Command("help"))
        if ADMIN_ID:
            self.dp.message.register(self._admin, Command("admin"))
            self.dp.message.register(self._delete, Command("delete"))
            self.dp.callback_query.register(self._cb_confirm_delete, F.data.startswith("confirm_delete_"))

        self.dp.callback_query.register(self._cb_register, F.data == "register")
        self.dp.callback_query.register(self._cb_login, F.data == "login")
        self.dp.callback_query.register(self._cb_buy, F.data == "buy_product")
        self.dp.callback_query.register(self._cb_pay, F.data.startswith("buy_confirm_"))
        self.dp.callback_query.register(self._cb_check, F.data == "check_payment")
        self.dp.callback_query.register(self._cb_back, F.data == "back_to_menu")
        self.dp.callback_query.register(self._cb_profile, F.data == "my_profile")
        self.dp.callback_query.register(self._cb_changepass, F.data == "change_password")
        self.dp.callback_query.register(self._cb_activate_key, F.data == "activate_key")
        if ADMIN_ID:
            self.dp.callback_query.register(self._cb_admin_users, F.data == "admin_users")
            self.dp.callback_query.register(self._cb_admin_stats, F.data == "admin_stats")
            self.dp.callback_query.register(self._cb_admin_keys, F.data == "admin_keys")
            self.dp.callback_query.register(self._cb_admin_key_plan, F.data.startswith("admin_key_"))

        from aiogram.filters import StateFilter
        self.dp.message.register(self._reg_login, StateFilter(RegisterState.waiting_for_login))
        self.dp.message.register(self._reg_password, StateFilter(RegisterState.waiting_for_password))
        self.dp.message.register(self._activate_key_message, StateFilter(KeyState.waiting_for_key))
        self.dp.message.register(self._admin_key_count_message, StateFilter(AdminKeyState.waiting_for_count))

    # ---------- Keyboard helpers ----------
    def _main_kb(self, registered=False):
        kb = []
        if not registered:
            kb.append([InlineKeyboardButton(text="🚀 Создать аккаунт", callback_data="register")])
            kb.append([InlineKeyboardButton(text="🔑 Уже есть аккаунт", callback_data="login")])
        else:
            kb.append([InlineKeyboardButton(text="💎 Купить / продлить доступ", callback_data="buy_product")])
            kb.append([InlineKeyboardButton(text="👤 Кабинет пользователя", callback_data="my_profile")])
        return InlineKeyboardMarkup(inline_keyboard=kb)

    def _back_kb(self):
        return InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="⬅️ Назад", callback_data="back_to_menu")]])

    def _plans_kb(self, prefix: str):
        return InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(
                text=f"{plan['emoji']} {plan['title']} — {plan['price']} USDT · {plan['badge']}",
                callback_data=f"{prefix}{plan_id}")]
            for plan_id, plan in SUBSCRIPTION_PLANS.items()
        ] + [[InlineKeyboardButton(text="⬅️ Назад", callback_data="back_to_menu")]])

    def _plans_text(self):
        lines = []
        for plan in SUBSCRIPTION_PLANS.values():
            lines.append(f"{plan['emoji']} <b>{plan['title']}</b> — <code>{plan['price']} USDT</code> · {plan['badge']}")
        return "\n".join(lines)

    # ---------- Commands ----------
    async def _start(self, m: Message):
        acc = await self.db.get_account_by_user_id(m.from_user.id)
        await m.answer(
            f"🌙 <b>Добро пожаловать, {m.from_user.first_name}!</b>\n\n"
            f"{PRODUCT_DESCRIPTION}\n\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"🛒 <b>Как начать:</b>\n"
            f"1. Создай аккаунт\n"
            f"2. Выбери срок подписки\n"
            f"3. Оплати или активируй ключ\n"
            f"4. Получи доступ к лоадеру\n\n"
            f"👇 Выбери действие:",
            reply_markup=self._main_kb(bool(acc)), parse_mode="HTML")

    async def _register(self, m: Message):
        await m.answer(
            "🚀 <b>Регистрация аккаунта</b>\n\n"
            "Создай личный кабинет для покупки и активации подписки.",
            reply_markup=InlineKeyboardMarkup(inline_keyboard=[
                [InlineKeyboardButton(text="📝 Начать регистрацию", callback_data="register")]]),
            parse_mode="HTML")

    async def _login(self, m: Message):
        await self._cb_login(m)

    async def _profile(self, m: Message):
        await self._cb_profile(m)

    async def _help(self, m: Message):
        t = (
            "📖 <b>Навигация по боту</b>\n\n"
            "🏠 /start — главное меню\n"
            "🚀 /register — регистрация\n"
            "🔑 /login — вход в кабинет\n"
            "👤 /profile — профиль и ключи\n"
            "❔ /help — помощь"
        )
        if ADMIN_ID: t += "\n\n🛠 <b>Админ:</b>\n/admin — панель\n/delete USER_ID — удалить аккаунт"
        await m.answer(t, parse_mode="HTML")

    async def _admin(self, m: Message):
        if m.from_user.id != ADMIN_ID: return
        await m.answer(
            "🛠 <b>Админ-панель</b>\n\n"
            "Управляй пользователями, доходом и ключами подписки.",
            reply_markup=InlineKeyboardMarkup(inline_keyboard=[
                [InlineKeyboardButton(text="👥 Пользователи", callback_data="admin_users")],
                [InlineKeyboardButton(text="📊 Статистика", callback_data="admin_stats")],
                [InlineKeyboardButton(text="🔐 Сгенерировать ключи", callback_data="admin_keys")],
            ]),
            parse_mode="HTML")

    async def _delete(self, m: Message):
        """/delete <user_id> — удалить аккаунт пользователя"""
        if m.from_user.id != ADMIN_ID: return
        parts = m.text.split()
        if len(parts) < 2:
            await m.answer("❌ Использование: <code>/delete USER_ID</code>\n\nПример: <code>/delete 123456789</code>", parse_mode="HTML")
            return
        try:
            user_id = int(parts[1])
        except ValueError:
            await m.answer("❌ USER_ID должен быть числом")
            return
        acc = await self.db.get_account_by_user_id(user_id)
        if not acc:
            await m.answer(f"❌ Аккаунт с user_id <code>{user_id}</code> не найден", parse_mode="HTML")
            return
        kb = InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text=f"✅ Удалить {acc['login']}", callback_data=f"confirm_delete_{user_id}")],
            [InlineKeyboardButton(text="❌ Отмена", callback_data="back_to_menu")],
        ])
        await m.answer(
            f"⚠️ <b>Удалить аккаунт?</b>\n\n"
            f"Логин: <code>{acc['login']}</code>\n"
            f"Оплачен: {'✅' if acc['is_paid'] else '❌'}\n"
            f"HWID: <code>{(acc.get('hwid') or '')[:32] or 'нет'}</code>",
            reply_markup=kb, parse_mode="HTML")

    async def _cb_confirm_delete(self, cb: CallbackQuery):
        if cb.from_user.id != ADMIN_ID: return
        user_id = int(cb.data.split("_")[-1])
        ok = await self.db.delete_account(user_id)
        if ok:
            await cb.message.edit_text(f"✅ Аккаунт пользователя <code>{user_id}</code> удалён", parse_mode="HTML")
        else:
            await cb.message.edit_text(f"❌ Не удалось удалить аккаунт", parse_mode="HTML")

    # ---------- Callbacks ----------
    async def _cb_register(self, cb: CallbackQuery, state: FSMContext):
        acc = await self.db.get_account_by_user_id(cb.from_user.id)
        if acc:
            await cb.message.edit_text(f"⚠️ Аккаунт уже есть: <code>{acc['login']}</code>\nДля восстановления — в поддержку.", parse_mode="HTML")
            return
        await cb.message.edit_text(
            "📝 <b>Регистрация</b>\n\n"
            "Введи <b>логин</b> для аккаунта.\n"
            "Допустимые символы: a-z, 0-9, _\n"
            "Минимум 4 символа.",
            parse_mode="HTML"
        )
        await cb.message.answer("👉 Введи логин:")
        await state.set_state(RegisterState.waiting_for_login)

    async def _reg_login(self, m: Message, state: FSMContext):
        login = m.text.strip()
        if len(login) < 4:
            await m.answer("❌ Логин слишком короткий. Минимум 4 символа.\n\n👉 Введи логин:")
            return
        if not all(c.isalnum() or c == '_' for c in login):
            await m.answer("❌ Логин: a-z, 0-9, _\n\n👉 Введи логин:")
            return

        existing = await self.db.get_account_by_login(login)
        if existing:
            await m.answer("❌ Логин занят.\n\n👉 Введи другой:")
            return

        await state.update_data(login=login)
        await state.set_state(RegisterState.waiting_for_password)
        await m.answer(
            "✅ Логин принят!\n\n"
            "Теперь введи <b>пароль</b>.\n"
            "Минимум 6 символов.",
            parse_mode="HTML"
        )

    async def _reg_password(self, m: Message, state: FSMContext):
        password = m.text.strip()
        if len(password) < 6:
            await m.answer("❌ Пароль слишком короткий. Минимум 6 символов.\n\n👉 Введи пароль:")
            return

        data = await state.get_data()
        login = data.get("login")
        await self.db.create_account(m.from_user.id, login, hash_password(password))
        await state.clear()

        kb = InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="💳 Купить доступ", callback_data="buy_product")],
            [InlineKeyboardButton(text="👤 Мой профиль", callback_data="my_profile")],
        ])
        await m.answer(
            f"🎉 <b>Аккаунт успешно создан!</b>\n\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"🔑 Логин: <code>{login}</code>\n"
            f"🛡 Статус: <b>ожидает подписку</b>\n"
            f"━━━━━━━━━━━━━━━━━━━━\n\n"
            f"Теперь можно выбрать тариф или активировать готовый ключ в профиле.",
            reply_markup=kb, parse_mode="HTML")

    async def _cb_login(self, obj):
        m = obj.message if isinstance(obj, CallbackQuery) else obj
        uid = obj.from_user.id if isinstance(obj, CallbackQuery) else m.from_user.id
        send = m.edit_text if isinstance(obj, CallbackQuery) else m.answer

        acc = await self.db.get_account_by_user_id(uid)
        if acc:
            await send(
                f"✅ <b>Вход выполнен</b>\n\n"
                f"👤 Логин: <code>{acc['login']}</code>\n"
                f"💎 Подписка: {'активна' if acc['is_paid'] else 'не активна'}\n"
                f"⏳ До: <code>{acc['subscription_expires'] or 'нет'}</code>",
                reply_markup=self._main_kb(True), parse_mode="HTML")
        else:
            await send("🔑 <b>Аккаунт не найден</b>\n\nСначала создай личный кабинет.",
                       reply_markup=InlineKeyboardMarkup(inline_keyboard=[
                           [InlineKeyboardButton(text="🚀 Создать аккаунт", callback_data="register")],
                           [InlineKeyboardButton(text="⬅️ Назад", callback_data="back_to_menu")],
                       ]), parse_mode="HTML")

    async def _cb_buy(self, cb: CallbackQuery):
        acc = await self.db.get_account_by_user_id(cb.from_user.id)
        if not acc:
            await cb.answer("Сначала зарегистрируйся!", show_alert=True)
            return
        await cb.message.edit_text(
            f"💎 <b>Выбор подписки</b>\n\n"
            f"👤 Аккаунт: <code>{acc['login']}</code>\n"
            f"📦 Доступ выдаётся сразу после оплаты.\n\n"
            f"{self._plans_text()}\n\n"
            f"👇 Выбери подходящий тариф:",
            reply_markup=self._plans_kb("buy_confirm_"), parse_mode="HTML")

    async def _cb_pay(self, cb: CallbackQuery):
        uid = cb.from_user.id
        plan_id = cb.data.replace("buy_confirm_", "", 1)
        plan = SUBSCRIPTION_PLANS.get(plan_id)
        if not plan:
            await cb.answer("Тариф не найден.", show_alert=True); return
        acc = await self.db.get_account_by_user_id(uid)
        if not acc:
            await cb.answer("Сначала зарегистрируйся!", show_alert=True); return
        try:
            res = await self.crypto.create_invoice(
                amount=plan["price"],
                description=f"Оплата {PRODUCT_NAME} на {plan['title']}",
                payload=f"buy_{acc['id']}_{uid}_{plan_id}")
            iid = res["invoice_id"]
            url = res["mini_app_url"]
            await self.db.add_payment(uid, iid, plan["price"], "USDT", "pending", plan_id)
            kb = InlineKeyboardMarkup(inline_keyboard=[
                [InlineKeyboardButton(text="💳 Оплатить", url=url)],
                [InlineKeyboardButton(text="🔄 Я оплатил", callback_data="check_payment")],
                [InlineKeyboardButton(text="⬅️ Назад", callback_data="back_to_menu")],
            ])
            await cb.message.edit_text(
                f"🧾 <b>Счёт создан</b>\n\n"
                f"━━━━━━━━━━━━━━━━━━━━\n"
                f"📌 Инвойс: <code>#{iid}</code>\n"
                f"{plan['emoji']} Тариф: <b>{plan['title']}</b>\n"
                f"💵 Сумма: <code>{plan['price']} USDT</code>\n"
                f"⏱ Статус: <b>ожидает оплаты</b>\n"
                f"━━━━━━━━━━━━━━━━━━━━\n\n"
                f"После оплаты нажми <b>«Я оплатил»</b>.",
                reply_markup=kb, parse_mode="HTML")
        except Exception as e:
            print(f"❌ {e}"); await cb.message.answer(f"❌ Ошибка: {e}")

    async def _cb_check(self, cb: CallbackQuery):
        uid = cb.from_user.id
        row = await self.db.get_pending_invoice(uid)
        if not row:
            await cb.answer("Нет ожидающих оплат.", show_alert=True); return
        iid = row[0]
        plan_id = row[1] or "1m"
        plan = SUBSCRIPTION_PLANS.get(plan_id, SUBSCRIPTION_PLANS["1m"])
        acc = await self.db.get_account_by_user_id(uid)
        if not acc:
            await cb.answer("Аккаунт не найден.", show_alert=True); return
        try:
            st = await self.crypto.get_invoice_status(iid)
            if st.get("status") == "paid":
                await self.db.update_payment_status(iid, "paid")
                expires = calculate_subscription_expires(acc.get("subscription_expires"), plan["days"])
                await self.db.set_paid(acc["id"], expires)
                if os.path.exists(LOADER_FILE_PATH):
                    await cb.message.answer_document(
                        document=FSInputFile(LOADER_FILE_PATH),
                        caption=f"✅ <b>Оплата подтверждена!</b>\n\nТариф: <b>{plan['title']}</b>\nПодписка до: <code>{expires}</code>\n\nСкачай лоадер, запусти и войди.\nHWID привяжется при первом входе.")
                else:
                    await cb.message.edit_text(
                        f"✅ Подписка активна до <code>{expires}</code>\n\nЛоадер: https://t.me/your_channel", parse_mode="HTML")
            elif st.get("status") == "expired":
                await self.db.update_payment_status(iid, "expired")
                await cb.answer("Инвойс истёк.", show_alert=True)
            else:
                await cb.answer("Ещё не оплачено.", show_alert=True)
        except Exception as e:
            await cb.answer(str(e), show_alert=True)

    async def _cb_profile(self, obj):
        m = obj.message if isinstance(obj, CallbackQuery) else obj
        uid = obj.from_user.id if isinstance(obj, CallbackQuery) else m.from_user.id
        send = m.edit_text if isinstance(obj, CallbackQuery) else m.answer
        acc = await self.db.get_account_by_user_id(uid)
        if not acc:
            await send("❌ Нет аккаунта. /register"); return
        status = "✅ Активна" if acc["is_paid"] else "❌ Не активна"
        hwid = (acc.get("hwid") or "")[:32] + "..." if len(acc.get("hwid") or "") > 32 else (acc.get("hwid") or "Не привязан")
        kb = InlineKeyboardMarkup(inline_keyboard=[
            [InlineKeyboardButton(text="🔑 Сменить пароль", callback_data="change_password")],
            [InlineKeyboardButton(text="🔐 Активировать ключ", callback_data="activate_key")],
            [InlineKeyboardButton(text="💎 Купить / продлить", callback_data="buy_product")],
            [InlineKeyboardButton(text="⬅️ Назад", callback_data="back_to_menu")],
        ])
        await send(
            f"👤 <b>Кабинет пользователя</b>\n\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"🆔 ID: <code>{uid}</code>\n"
            f"🔑 Логин: <code>{acc['login']}</code>\n"
            f"💎 Подписка: <b>{status}</b>\n"
            f"⏳ Истекает: <code>{acc['subscription_expires'] or 'нет'}</code>\n"
            f"🖥 HWID: <code>{hwid}</code>\n"
            f"━━━━━━━━━━━━━━━━━━━━\n\n"
            f"Здесь можно продлить доступ, активировать ключ или сменить пароль.",
            reply_markup=kb, parse_mode="HTML")

    async def _cb_activate_key(self, cb: CallbackQuery, state: FSMContext):
        acc = await self.db.get_account_by_user_id(cb.from_user.id)
        if not acc:
            await cb.answer("Сначала зарегистрируйся!", show_alert=True)
            return
        await cb.message.edit_text(
            "🔐 <b>Активация ключа</b>\n\n"
            "━━━━━━━━━━━━━━━━━━━━\n"
            "Ключ выдаёт подписку на выбранный срок и активируется только один раз.\n\n"
            "Введи ключ формата <code>ar_...</code>:",
            reply_markup=self._back_kb(), parse_mode="HTML")
        await state.set_state(KeyState.waiting_for_key)

    async def _activate_key_message(self, m: Message, state: FSMContext):
        key_value = m.text.strip()
        acc = await self.db.get_account_by_user_id(m.from_user.id)
        if not acc:
            await state.clear()
            await m.answer("❌ Нет аккаунта. /register")
            return
        key = await self.db.get_subscription_key(key_value)
        if not key:
            await m.answer("❌ Ключ не найден.\n\nВведи ключ ещё раз или нажми /profile")
            return
        if key["is_used"]:
            await m.answer("❌ Этот ключ уже активирован.\n\nВведи другой ключ или нажми /profile")
            return
        plan = SUBSCRIPTION_PLANS.get(key["plan_id"])
        if not plan:
            await m.answer("❌ У ключа неизвестный тариф. Обратись к администратору.")
            return
        expires = calculate_subscription_expires(acc.get("subscription_expires"), plan["days"])
        await self.db.set_paid(acc["id"], expires)
        await self.db.use_subscription_key(key["id"], m.from_user.id)
        await state.clear()
        await m.answer(
            f"✅ <b>Ключ активирован!</b>\n\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"{plan['emoji']} Тариф: <b>{plan['title']}</b>\n"
            f"⏳ Подписка до: <code>{expires}</code>\n"
            f"━━━━━━━━━━━━━━━━━━━━\n\n"
            f"Доступ обновлён. Приятного использования!",
            reply_markup=self._main_kb(True), parse_mode="HTML")

    async def _cb_changepass(self, cb: CallbackQuery):
        acc = await self.db.get_account_by_user_id(cb.from_user.id)
        if not acc: return
        pwd = generate_password()
        await self.db.change_password(acc["id"], hash_password(pwd))
        await cb.message.edit_text(
            f"🔑 Пароль изменён!\n\nЛогин: <code>{acc['login']}</code>\nПароль: <code>{pwd}</code>",
            reply_markup=self._back_kb(), parse_mode="HTML")

    async def _cb_back(self, cb: CallbackQuery, state: FSMContext):
        await state.clear()
        acc = await self.db.get_account_by_user_id(cb.from_user.id)
        await cb.message.edit_text(
            "🏠 <b>Главное меню</b>\n\n"
            "Выбери нужное действие ниже:",
            reply_markup=self._main_kb(bool(acc)), parse_mode="HTML")

    async def _cb_admin_users(self, cb: CallbackQuery):
        if cb.from_user.id != ADMIN_ID: return
        users = await self.db.get_all_accounts()
        t = f"👥 <b>Аккаунты</b> · всего: <b>{len(users)}</b>\n\n"
        for u in users[-30:]:
            s = "✅" if u["is_paid"] else "❌"
            t += f"{s} <code>{u['login']}</code> · HWID: {(u.get('hwid') or '')[:16] or 'нет'} · до: {u.get('subscription_expires') or 'нет'}\n"
        await cb.message.answer(t, parse_mode="HTML")

    async def _cb_admin_stats(self, cb: CallbackQuery):
        if cb.from_user.id != ADMIN_ID: return
        s = await self.db.get_stats()
        await cb.message.answer(
            f"📊 <b>Статистика проекта</b>\n\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"👥 Пользователей: <b>{s['total']}</b>\n"
            f"💎 С подпиской: <b>{s['paid']}</b>\n"
            f"💰 Доход: <code>{s['revenue']} USDT</code>\n"
            f"━━━━━━━━━━━━━━━━━━━━",
            parse_mode="HTML")

    async def _cb_admin_keys(self, cb: CallbackQuery):
        if cb.from_user.id != ADMIN_ID: return
        await cb.message.edit_text(
            "🔐 <b>Генератор ключей</b>\n\n"
            "Создай одноразовые ключи формата <code>ar_...</code> для нужного срока подписки.\n\n"
            "👇 Выбери срок:",
            reply_markup=self._plans_kb("admin_key_"), parse_mode="HTML")

    async def _cb_admin_key_plan(self, cb: CallbackQuery, state: FSMContext):
        if cb.from_user.id != ADMIN_ID: return
        plan_id = cb.data.replace("admin_key_", "", 1)
        plan = SUBSCRIPTION_PLANS.get(plan_id)
        if not plan:
            await cb.answer("Тариф не найден.", show_alert=True)
            return
        await state.update_data(plan_id=plan_id)
        await state.set_state(AdminKeyState.waiting_for_count)
        await cb.message.edit_text(
            f"🔐 <b>Генерация ключей</b>\n\n"
            f"{plan['emoji']} Тариф: <b>{plan['title']}</b>\n"
            f"🏷 Метка: <b>{plan['badge']}</b>\n\n"
            f"Введи количество ключей от 1 до 50:",
            reply_markup=self._back_kb(), parse_mode="HTML")

    async def _admin_key_count_message(self, m: Message, state: FSMContext):
        if m.from_user.id != ADMIN_ID: return
        try:
            count = int(m.text.strip())
        except ValueError:
            await m.answer("❌ Введи число от 1 до 50:")
            return
        if count < 1 or count > 50:
            await m.answer("❌ Количество должно быть от 1 до 50:")
            return
        data = await state.get_data()
        plan_id = data.get("plan_id")
        plan = SUBSCRIPTION_PLANS.get(plan_id)
        if not plan:
            await state.clear()
            await m.answer("❌ Тариф не найден.")
            return
        keys = []
        for _ in range(count):
            keys.append(await self.db.create_subscription_key(plan_id, m.from_user.id))
        await state.clear()
        keys_text = "\n".join(f"<code>{key}</code>" for key in keys)
        await m.answer(
            f"✅ <b>Ключи созданы</b>\n\n"
            f"{plan['emoji']} Тариф: <b>{plan['title']}</b>\n"
            f"Количество: <b>{count}</b>\n\n"
            f"{keys_text}",
            parse_mode="HTML")

    # ---------- Run ----------
    async def run(self):
        await self.db.init()
        os.makedirs("files", exist_ok=True)
        os.makedirs("data", exist_ok=True)
        print(f"✅ {PRODUCT_NAME} bot started")
        await self.dp.start_polling(self.bot)

    async def shutdown(self):
        await self.crypto.close()
        await self.db.close()

async def main():
    if not BOT_TOKEN: print("❌ BOT_TOKEN"); return
    if not CRYPTO_PAY_API_KEY: print("❌ CRYPTO_PAY_API_KEY"); return
    bot = OyuzBot()
    try: await bot.run()
    except KeyboardInterrupt: print("\n👋 Bye")
    finally: await bot.shutdown()

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    asyncio.run(main())
