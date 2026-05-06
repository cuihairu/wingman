import { PageContainer } from '@ant-design/pro-components';
import { history } from '@umijs/max';
import {
  ApiOutlined,
  ArrowRightOutlined,
  BlockOutlined,
  CheckCircleOutlined,
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
                  <Tag color="blue">多节点编排</Tag>
                  <Tag color="cyan">工作流引擎</Tag>
                  <Tag color="purple">实时监控</Tag>
                </Space>
                <Typography.Title level={2} style={{ margin: 0 }}>
                  Wingman 游戏自动化控制引擎
                </Typography.Title>
                <Typography.Paragraph
                  type="secondary"
                  style={{ margin: 0, fontSize: 16, lineHeight: 1.8 }}
                >
                  分布式自动化控制平台，支持多 Agent 协同工作、可视化工作流编排、实时任务监控。
                  通过 Lua 脚本实现灵活的游戏自动化操作。
                </Typography.Paragraph>
                <Space wrap size={[12, 12]}>
                  <Button
                    type="primary"
                    icon={<RocketOutlined />}
                    size="large"
                    onClick={() => history.push('/workflows')}
                  >
                    创建工作流
                  </Button>
                  <Button
                    size="large"
                    icon={<DesktopOutlined />}
                    onClick={() => history.push('/agents')}
                  >
                    管理 Agent
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
                  <Typography.Text strong>快速开始</Typography.Text>
                  <Space wrap size={[8, 8]}>
                    <Tag icon={<CheckCircleOutlined />} color="success">
                      Agent 已就绪
                    </Tag>
                    <Tag icon={<CheckCircleOutlined />} color="success">
                      工作流引擎运行中
                    </Tag>
                  </Space>
                  <Typography.Text type="secondary">
                    系统正常运行，您可以开始创建工作流或管理 Agent 节点。
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
              title="Agent 管理"
              description="查看所有已注册的 Agent 节点，监控 CPU、内存、网络等资源使用情况，管理 Agent 状态。"
              icon={<NodeIndexOutlined />}
              actionLabel="管理节点"
              path="/agents"
              tone="linear-gradient(135deg, #1668dc 0%, #69b1ff 100%)"
            />
          </Col>
          <Col xs={24} md={8}>
            <EntryCard
              title="工作流编排"
              description="创建和管理自动化工作流，支持多步骤任务编排、依赖配置、并行执行和失败重试。"
              icon={<BlockOutlined />}
              actionLabel="创建工作流"
              path="/workflows"
              tone="linear-gradient(135deg, #722ed1 0%, #b37feb 100%)"
            />
          </Col>
          <Col xs={24} md={8}>
            <EntryCard
              title="实时监控"
              description="实时查看工作流执行进度、Agent 状态、任务日志和系统资源使用情况。"
              icon={<DesktopOutlined />}
              actionLabel="查看监控"
              path="/agents"
              tone="linear-gradient(135deg, #0f9d58 0%, #34d399 100%)"
            />
          </Col>
        </Row>

        {/* 使用流程 */}
        <Card title="使用流程" bordered={false}>
          <Row gutter={[16, 16]}>
            <Col xs={24} md={8}>
              <Card size="small" type="inner">
                <Space direction="vertical" size={12}>
                  <Tag color="blue">步骤 1</Tag>
                  <Typography.Title level={5}>注册 Agent</Typography.Title>
                  <Typography.Text type="secondary">
                    启动 Wingman 客户端，自动注册到服务器。在 Agent 管理页面查看所有在线节点。
                  </Typography.Text>
                  <Button size="small" onClick={() => history.push('/agents')}>
                    查看 Agent
                  </Button>
                </Space>
              </Card>
            </Col>
            <Col xs={24} md={8}>
              <Card size="small" type="inner">
                <Space direction="vertical" size={12}>
                  <Tag color="blue">步骤 2</Tag>
                  <Typography.Title level={5}>创建工作流</Typography.Title>
                  <Typography.Text type="secondary">
                    在工作流管理页面创建任务流，配置执行步骤、依赖关系和超时时间。
                  </Typography.Text>
                  <Button type="primary" size="small" onClick={() => history.push('/workflows')}>
                    创建工作流
                  </Button>
                </Space>
              </Card>
            </Col>
            <Col xs={24} md={8}>
              <Card size="small" type="inner">
                <Space direction="vertical" size={12}>
                  <Tag color="blue">步骤 3</Tag>
                  <Typography.Title level={5}>监控执行</Typography.Title>
                  <Typography.Text type="secondary">
                    实时监控工作流执行状态，查看步骤进度和 Agent 资源使用情况。
                  </Typography.Text>
                  <Button size="small" onClick={() => history.push('/workflows')}>
                    查看工作流
                  </Button>
                </Space>
              </Card>
            </Col>
          </Row>
        </Card>

        {/* 技术特性 */}
        <Card title="技术特性" bordered={false}>
          <Row gutter={[16, 16]}>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <ApiOutlined style={{ fontSize: 24, color: token.colorPrimary }} />
                <Typography.Text strong>Protobuf 通信</Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  高效二进制协议
                </Typography.Text>
              </Space>
            </Col>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <NodeIndexOutlined style={{ fontSize: 24, color: token.colorSuccess }} />
                <Typography.Text strong>分布式节点</Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  多 Agent 协同
                </Typography.Text>
              </Space>
            </Col>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <BlockOutlined style={{ fontSize: 24, color: token.colorWarning }} />
                <Typography.Text strong>工作流引擎</Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  DAG 任务编排
                </Typography.Text>
              </Space>
            </Col>
            <Col xs={12} sm={6}>
              <Space direction="vertical" size={4}>
                <DesktopOutlined style={{ fontSize: 24, color: token.colorError }} />
                <Typography.Text strong>Lua 脚本</Typography.Text>
                <Typography.Text type="secondary" style={{ fontSize: 12 }}>
                  灵活任务定义
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
