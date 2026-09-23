import type { GetStaticPaths, GetStaticProps, InferGetStaticPropsType } from 'next'
import Layout from '@/components/Layout'
import { getDoc, getSlugs, type Doc } from '@/lib/markdown'
import styles from '@/styles/Layout.module.css'

export const getStaticPaths: GetStaticPaths = async () => {
  return {
    paths: getSlugs().map((slug) => ({ params: { slug } })),
    fallback: false,
  }
}

export const getStaticProps: GetStaticProps<{ doc: Doc }, { slug: string }> = async ({ params }) => {
  return { props: { doc: await getDoc(params!.slug) } }
}

export default function DocPage({ doc }: InferGetStaticPropsType<typeof getStaticProps>) {
  return (
    <Layout lang="en" path={`/${doc.slug}`} title={`${doc.title} - Octopass`} description={doc.description}>
      <main className={`markdown ${styles.article}`} dangerouslySetInnerHTML={{ __html: doc.html }} />
    </Layout>
  )
}
