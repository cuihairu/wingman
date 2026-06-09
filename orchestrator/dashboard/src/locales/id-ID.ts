import component from './id-ID/component';
import globalHeader from './id-ID/globalHeader';
import menu from './id-ID/menu';
import pages from './id-ID/pages';
import pwa from './id-ID/pwa';
import settingDrawer from './id-ID/settingDrawer';
import settings from './id-ID/settings';

export default {
  'navbar.lang': 'Bahasa',
  'layout.user.link.help': 'Bantuan',
  'layout.user.link.privacy': 'Privasi',
  'layout.user.link.terms': 'Ketentuan',
  'app.preview.down.block': 'Unduh halaman ini dalam projek lokal anda',
  'app.welcome.link.fetch-blocks': 'Dapatkan semua blok',
  'app.welcome.link.block-list':
    'Buat standar dengan cepat, halaman-halaman berdasarkan pengembangan `block`',
  ...globalHeader,
  ...menu,
  ...settingDrawer,
  ...settings,
  ...pwa,
  ...component,
  ...pages,
  'pages.layouts.userLayout.title':
    'Wingman untuk orkestrasi, otomasi, dan audit operasional',
  'pages.login.accountLogin.errorMessage': 'Nama pengguna atau kata sandi salah',
  'pages.login.username.placeholder': 'Nama pengguna',
  'pages.login.password.placeholder': 'Kata sandi',
};
