import component from './pt-BR/component';
import globalHeader from './pt-BR/globalHeader';
import menu from './pt-BR/menu';
import pages from './pt-BR/pages';
import pwa from './pt-BR/pwa';
import settingDrawer from './pt-BR/settingDrawer';
import settings from './pt-BR/settings';

export default {
  'navBar.lang': 'Idiomas',
  'layout.user.link.help': 'Ajuda',
  'layout.user.link.privacy': 'Política de privacidade',
  'layout.user.link.terms': 'Termos de serviço',
  'app.preview.down.block': 'Baixe esta página para o seu projeto local',
  ...globalHeader,
  ...menu,
  ...settingDrawer,
  ...settings,
  ...pwa,
  ...component,
  ...pages,
  'pages.layouts.userLayout.title': 'Wingman para orquestração, automação e auditoria operacional',
  'pages.login.accountLogin.errorMessage': 'Usuário ou senha incorretos',
  'pages.login.username.placeholder': 'Usuário',
  'pages.login.password.placeholder': 'Senha',
};
