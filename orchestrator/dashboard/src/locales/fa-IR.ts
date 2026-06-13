import component from './fa-IR/component';
import globalHeader from './fa-IR/globalHeader';
import menu from './fa-IR/menu';
import pages from './fa-IR/pages';
import pwa from './fa-IR/pwa';
import settingDrawer from './fa-IR/settingDrawer';
import settings from './fa-IR/settings';

export default {
  'navBar.lang': 'زبان‌ها',
  'layout.user.link.help': 'راهنما',
  'layout.user.link.privacy': 'حریم خصوصی',
  'layout.user.link.terms': 'شرایط استفاده',
  'app.preview.down.block': 'این صفحه را در پروژه محلی خود دانلود کنید',
  'app.welcome.link.fetch-blocks': 'دریافت همه بلوک‌ها',
  'app.welcome.link.block-list': 'با توسعه مبتنی بر block به‌سرعت صفحه‌های استاندارد بسازید',
  ...globalHeader,
  ...menu,
  ...settingDrawer,
  ...settings,
  ...pwa,
  ...component,
  ...pages,
  'pages.layouts.userLayout.title': 'Wingman برای ارکستراسیون، خودکارسازی و ممیزی عملیاتی',
  'pages.login.accountLogin.errorMessage': 'نام کاربری یا رمز عبور نادرست است',
  'pages.login.username.placeholder': 'نام کاربری',
  'pages.login.password.placeholder': 'رمز عبور',
};
