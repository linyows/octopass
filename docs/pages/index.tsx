import type { GetStaticProps, InferGetStaticPropsType } from 'next'
import Layout from '@/components/Layout'
import { getDoc, type Doc } from '@/lib/markdown'
import styles from '@/styles/Layout.module.css'

export const getStaticProps: GetStaticProps<{ doc: Doc }> = async () => {
  return { props: { doc: await getDoc('index') } }
}

export default function Home({ doc }: InferGetStaticPropsType<typeof getStaticProps>) {
  return (
    <Layout lang="en" path="/" title={doc.title} description={doc.description}>
      <main className={`markdown ${styles.columns}`} dangerouslySetInnerHTML={{ __html: doc.html }} />
    </Layout>
  )
}
