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
  'layout.user.link.terms': 'শর্তাদি',
  'app.preview.down.block': 'আপনার স্থানীয় প্রকল্পে এই পৃষ্ঠাটি ডাউনলোড করুন',
  'app.welcome.link.fetch-blocks': 'সমস্ত ব্লক পান',
  'app.welcome.link.block-list':
    '`block` ডেভেলপমেন্ট এর উপর ভিত্তি করে দ্রুত স্ট্যান্ডার্ড, পৃষ্ঠাসমূহ তৈরি করুন।',
  ...globalHeader,
  ...menu,
  ...settingDrawer,
  ...settings,
  ...pwa,
  ...component,
  ...pages,
  'pages.layouts.userLayout.title': 'Wingman অর্কেস্ট্রেশন, অটোমেশন এবং অডিট কন্ট্রোল প্লেন',
  'pages.login.accountLogin.errorMessage': 'ব্যবহারকারীর নাম বা পাসওয়ার্ড সঠিক নয়',
  'pages.login.username.placeholder': 'ব্যবহারকারীর নাম',
  'pages.login.password.placeholder': 'পাসওয়ার্ড',
};
