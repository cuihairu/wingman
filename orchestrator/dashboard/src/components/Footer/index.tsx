import { GithubOutlined } from '@ant-design/icons';
import { DefaultFooter } from '@ant-design/pro-components';
import React from 'react';
import { BRAND } from '@/config/branding';

const Footer: React.FC = () => {
  return (
    <DefaultFooter
      style={{
        background: 'none',
      }}
      copyright={`${new Date().getFullYear()} ${BRAND.title}`}
      links={[
        {
          key: 'wingman',
          title: (
            <span>
              <GithubOutlined style={{ marginRight: 6 }} /> Wingman
            </span>
          ),
          href: 'https://github.com/cuihairu/wingman',
          blankTarget: true,
        },
        {
          key: 'docs',
          title: '文档',
          href: 'https://github.com/cuihairu/wingman/tree/main/docs',
          blankTarget: true,
        },
      ]}
    />
  );
};

export default Footer;
