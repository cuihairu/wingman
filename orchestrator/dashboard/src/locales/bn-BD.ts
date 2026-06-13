import component from './bn-BD/component';
import globalHeader from './bn-BD/globalHeader';
import menu from './bn-BD/menu';
import pages from './bn-BD/pages';
import pwa from './bn-BD/pwa';
import settingDrawer from './bn-BD/settingDrawer';
import settings from './bn-BD/settings';

export default {
  'navBar.lang': 'ভাষা',
  'layout.user.link.help': 'সহায়তা',
  'layout.user.link.privacy': 'গোপনীয়তা',
  'layout.user.link.terms': 'শর্তাবলী',
  'app.preview.down.block': 'এই পৃষ্ঠাটি আপনার লোকাল প্রজেক্টে ডাউনলোড করুন',
  'app.welcome.link.fetch-blocks': 'সব ব্লক আনুন',
  'app.welcome.link.block-list':
    '`block`-ভিত্তিক ডেভেলপমেন্ট দিয়ে দ্রুত স্ট্যান্ডার্ড পৃষ্ঠা তৈরি করুন',
  ...globalHeader,
  ...menu,
  ...settingDrawer,
  ...settings,
  ...pwa,
  ...component,
  ...pages,
  'pages.layouts.userLayout.title': 'Wingman অর্কেস্ট্রেশন, অটোমেশন এবং অডিট কন্ট্রোল প্লেন',
  'pages.login.accountLogin.errorMessage': 'ব্যবহারকারীর নাম বা পাসওয়ার্ড ভুল',
  'pages.login.username.placeholder': 'ব্যবহারকারীর নাম',
  'pages.login.password.placeholder': 'পাসওয়ার্ড',
};
