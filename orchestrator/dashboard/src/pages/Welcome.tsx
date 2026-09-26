import { PageContainer } from '@ant-design/pro-components';
import { history, useIntl } from '@umijs/max';
import {
  ApiOutlined,
  ArrowRightOutlined,
  BlockOutlined,
  DesktopOutlined,
  RocketOutlined,
  NodeIndexOutlined,
} from '@ant-design/icons';
import { Button, Card, Col, Row, Space, Tag, Typography, theme } from 'antd';
import React from 'react';

type EntryCardProps = {
  title: string;
  description: string;
  icon: React.ReactNode;
  actionLabel: string;
  path: string;
  tone: string;
};

function EntryCard({ title, description, icon, actionLabel, path, tone }: EntryCardProps) {
  return (
    <Card
      hoverable
      style={{ height: '100%' }}
      styles={{ body: { height: '100%' } }}
      onClick={() => history.push(path)}
    >
      <Space direction="vertical" size={16} style={{ width: '100%', height: '100%' }}>
        <Space size={12}>
          <div
            style={{
              width: 48,
              height: 48,
              borderRadius: 12,
              display: 'grid',
              placeItems: 'center',
              background: tone,
              color: '#fff',
              fontSize: 22,
            }}
          >
            {icon}
          </div>
          <Typography.Title level={4} style={{ margin: 0 }}>
            {title}
          </Typography.Title>
        </Space>
        <Typography.Paragraph type="secondary" style={{ marginBottom: 0, flex: 1 }}>
          {description}
        </Typography.Paragraph>
        <Button type="link" style={{ padding: 0 }} icon={<ArrowRightOutlined />}>
          {actionLabel}
        </Button>
      </Space>
    </Card>
  );
}

const Welcome: React.FC = () => {
  const { token } = theme.useToken();
  const intl = useIntl();
  const formatMessage = (id: string) => intl.formatMessage({ id });

  return (
    <PageContainer
      header={{
        title: false,
        breadcrumb: {},
      }}
    >
      <Space direction="vertical" size={20} style={{ width: '100%' }}>
        {/* 主卡片 */}
        <Card
          bordered={false}
          style={{
            overflow: 'hidden',
            background: `linear-gradient(135deg, ${token.colorBgContainer} 0%, ${token.colorPrimaryBg} 100%)`,
          }}
        >
          <Row gutter={[24, 24]} align="middle">
            <Col xs={24} xl={16}>
              <Space direction="vertical" size={18} style={{ width: '100%' }}>
                <Space wrap>
                  <Tag color="blue">{formatMessage('pages.welcome.tagOrchestration')}</Tag>
                  <Tag color="cyan">{formatMessage('pages.welcome.tagWorkflow')}</Tag>
                  <Tag color="purple">{formatMessage('pages.welcome.tagMonitor')}</Tag>
                </Space>
                <Typography.Title level={2} style={{ margin: 0 }}>
                  {formatMessage('pages.welcome.heroTitle')}
                </Typography.Title>
                <Typography.Paragraph
                  type="secondary"
                  style={{ margin: 0, fontSize: 16, lineHeight: 1.8 }}
                >
                  {formatMessage('pages.welcome.heroDescription')}
                </Typography.Paragraph>
                <Space wrap size={[12, 12]}>
                  <Button
                    type="primary"
                    icon={<RocketOutlined />}
                    size="large"
                    onClick={() => history.push('/workflows')}
                  >
                    {formatMessage('pages.welcome.createWorkflow')}
                  </Button>
                  <Button
                    size="large"
                    icon={<DesktopOutlined />}
                    onClick={() => history.push('/agents')}
                  >
                    {formatMessage('pages.welcome.manageAgents')}
                  </Button>
                </Space>
              </Space>
            </Col>
            <Col xs={24} xl={8}>
              <Card
                size="small"
                style={{ background: 'rgba(255,255,255,0.65)', borderColor: token.colorBorder }}
              >
                <Space direction="vertical" size={10} style={{ width: '100%' }}>
                  <Typography.Text strong>
                    {formatMessage('pages.welcome.quickStart')}
                  </Typography.Text>
                  <Space wrap size={[8, 8]}>
                    <Tag color="blue">{formatMessage('pages.welcome.tagAgentAccess')}</Tag>
                    <Tag color="geekblue">{formatMessage('pages.welcome.tagScriptDelivery')}</Tag>
                    <Tag color="purple">
                      {formatMessage('pages.welcome.tagWorkflowOrchestration')}
                    </Tag>
                  </Space>
                  <Typography.Text type="secondary">
                    {formatMessage('pages.welcome.quickStartHint')}
                  </Typography.Text>
                </Space>
              </Card>
            </Col>
          </Row>
        </Card>

        {/* 功能卡片 */}
        <Row gutter={[16, 16]}>
          <Col xs={24} md={8}>
            <EntryCard
              title={formatMessage('pages.welcome.monitorCardTitle')}
              description={formatMessage('pages.welcome.monitorCardDescription')}
              icon={<DesktopOutlined />}
              actionLabel={formatMessage('pages.welcome.monitorCardAction')}
              path="/monitor"
              tone="linear-gradient(135deg, #0f9d58 0%, #34d399 100%)"
            />
          </Col>
          <Col xs={24} md={8}>
            <EntryCard
              title={formatMessage('pages.welcome.agentsCardTitle')}
              description={formatMessage('pages.welcome.agentsCardDescription')}
              icon={<NodeIndexOutlined />}
              actionLabel={formatMessage('pages.welcome.agentsCardAction')}
              path="/agents"
              tone="linear-gradient(135deg, #1668dc 0%, #69b1ff 100%)"
            />
          </Col>
          <Col xs={24} md={8}>
            <EntryCard
              title={formatMessage('pages.welcome.workflowsCardTitle')}
              description={formatMessage('pages.welcome.workflowsCardDescription')}
              icon={<BlockOutlined />}
              actionLabel={formatMessage('pages.welcome.workflowsCardAction')}
              path="/workflows"
              tone="linear-gradient(135deg, #722ed1 0%, #b37feb 100%)"
            />
          </Col>
        </Row>

        {/* 使用流程 */}
        <Card title={formatMessage('pages.welcome.usageTitle')} bordered={false}>
          <Row gutter={[16, 16]}>
            <Col xs={24} md={8}>
              <Card size="small" type="inner">
                <Space direction="vertical" size={12}>
                  <Tag color="blue">{formatMessage('pages.welcome.step1Tag')}</Tag>
                  <Typography.Title level={5}>
                    {formatMessage('pages.welcome.step1Title')}
                  </Typography.Title>
                  <Typography.Text type="secondary">
                    {formatMessage('pages.welcome.step1Description')}
                  </Typography.Text>
                  <Button size="small" onClick={() => history.push('/agents')}>
                    {formatMessage('pages.welcome.step1Action')}
                  </Button>
                </Space>
              </Card>
            </Col>
            <Col xs={24} md={8}>
              <Card size="small" type="inner">
                <Space direction="vertical" size={12}>
                  <Tag color="blue">{formatMessage('pages.welcome.step2Tag')}</Tag>
                  <Typography.Title level={5}>
                    {formatMessage('pages.welcome.step2Title')}
                  </Typography.Title>
                  <Typography.Text type="secondary">
                    {formatMessage('pages.welcome.step2Description')}
                  </Typography.Text>
                  <Button type="primary" size="small" onClick={() => history.push('/workflows')}>
                    {formatMessage('pages.welcome.step2Action')}
                  </Button>
                </Space>
              </Card>
            </Col>
            <Col xs={24} md={8}>
              <Card size="small" type="inner">
                <Space direction="vertical" size={12}>
                  <Tag color="blue">{formatMessage('pages.welcome.step3Tag')}</Tag>
                  <Typography.Title level={5}>
                    {formatMessage('pages.welcome.step3Title')}
                  </Typography.Title>
                  <Typography.Text type="secondary">
                    {formatMessage('pages.welcome.step3Description')}
                  </Typography.Text>
                  <Button size="small" onClick={() => history.push('/workflows')}>
                    {formatMessage('pages.welcome.step3Action')}
                  </Button>
                </Space>
              </Card>
            </Col>
          </Row>
        </Card>

        {/* 技术特性 */}
        <Card title={formatMessage('pages.welcome.techTitle')} bordered={false}>
          <Row gutter={[16, 16]}>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <ApiOutlined style={{ fontSize: 24, color: token.colorPrimary }} />
                <Typography.Text strong>
                  {formatMessage('pages.welcome.techProtobuf')}
                </Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  {formatMessage('pages.welcome.techProtobufDescription')}
                </Typography.Text>
              </Space>
            </Col>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <NodeIndexOutlined style={{ fontSize: 24, color: token.colorSuccess }} />
                <Typography.Text strong>
                  {formatMessage('pages.welcome.techDistributedNodes')}
                </Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  {formatMessage('pages.welcome.techDistributedNodesDescription')}
                </Typography.Text>
              </Space>
            </Col>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <BlockOutlined style={{ fontSize: 24, color: token.colorWarning }} />
                <Typography.Text strong>
                  {formatMessage('pages.welcome.techWorkflowEngine')}
                </Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  {formatMessage('pages.welcome.techWorkflowEngineDescription')}
                </Typography.Text>
              </Space>
            </Col>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <DesktopOutlined style={{ fontSize: 24, color: token.colorError }} />
                <Typography.Text strong>
                  {formatMessage('pages.welcome.techLuaScript')}
                </Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  {formatMessage('pages.welcome.techLuaScriptDescription')}
                </Typography.Text>
              </Space>
            </Col>
          </Row>
        </Card>
      </Space>
    </PageContainer>
  );
};

export default Welcome;
