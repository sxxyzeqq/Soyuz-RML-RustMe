import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    SECRET_KEY = os.getenv('SECRET_KEY', 'change-this-to-random-secret-key')
    DATABASE = os.getenv('DATABASE', 'instance/soyuz.db')
    
    # YooMoney Configuration
    YOOMONEY_CLIENT_ID = os.getenv('YOOMONEY_CLIENT_ID', '')
    YOOMONEY_REDIRECT_URI = os.getenv('YOOMONEY_REDIRECT_URI', 'http://localhost:5000/payment/callback')
    YOOMONEY_SHOP_ID = os.getenv('YOOMONEY_SHOP_ID', '')
    
    # Admin Configuration
    ADMIN_USERNAME = os.getenv('ADMIN_USERNAME', 'admin')
    ADMIN_PASSWORD_HASH = os.getenv('ADMIN_PASSWORD_HASH', '')
    
    # Subscription Prices (in RUB)
    PRICES = {
        'week': 149,
        'month': 399,
        'lifetime': 1499
    }
    
    # Subscription Durations (in days)
    DURATIONS = {
        'week': 7,
        'month': 30,
        'lifetime': 36500  # ~100 years
    }
