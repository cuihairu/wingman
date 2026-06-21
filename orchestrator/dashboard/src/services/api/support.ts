import { fetchJSON } from '@/services/core/http';

/**
 * Support API
 */

export interface CreateFeedbackParams {
  category: string;
  content: string;
  priority?: string;
  source?: string;
}

export async function createFeedback(params: CreateFeedbackParams): Promise<void> {
  await fetchJSON('/api/feedback', {
    method: 'POST',
    body: JSON.stringify(params),
  });
}
