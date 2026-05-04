import { ConfigProvider } from 'antd';
import zhCN from 'antd/locale/zh_CN';
import './global.less';

export function rootContainer(container: any) {
  return (
    <ConfigProvider locale={zhCN}>
      {container}
    </ConfigProvider>
  );
}
