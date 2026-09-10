# $oyuz Web App

Веб-приложение для управления подписками и продажи лоадера.

## Установка

```bash
cd web
pip install -r requirements.txt
copy .env.example .env
```

## Настройка .env

```
DB_PATH=../backup/bot/data/users.db
YOOMONEY_TOKEN=ваш_токен
YOOMONEY_RECEIVER=ваш_кошелёк
SECRET_KEY=случайная_строка
ADMIN_LOGIN=admin
ADMIN_PASSWORD_HASH=хеш_пароля
LOADER_FILE_PATH=../files/system32.exe
```

## Получение токена ЮMoney

1. Зайди на [yoomoney.ru](https://yoomoney.ru)
2. Перейди в [OAuth](https://yoomoney.ru/oauth/authorize?response_type=token&client_id=ваш_client_id)
3. Получи токен с правами на историю операций

## Запуск

```bash
python app.py
```

Открой http://localhost:5000

## Админка

Логин: `admin` / Пароль: `admin123` (измени в .env)

## API для лоадера

```
POST /api/login
Body: {"login": "...", "password": "...", "hwid": "..."}
```
