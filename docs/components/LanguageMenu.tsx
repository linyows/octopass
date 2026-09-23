import { useEffect, useRef } from 'react'
import Link from 'next/link'
import styles from '@/styles/LanguageMenu.module.css'

export type Lang = 'en' | 'ja'

const languages: { code: Lang; label: string }[] = [
  { code: 'en', label: 'English' },
  { code: 'ja', label: '日本語' },
]

export function localizedPath(lang: Lang, path: string): string {
  if (lang === 'en') return path
  return path === '/' ? `/${lang}` : `/${lang}${path}`
}

type Props = {
  lang: Lang
  path: string
}

export default function LanguageMenu({ lang, path }: Props) {
  const ref = useRef<HTMLDetailsElement>(null)
  const current = languages.find((l) => l.code === lang)!

  // Close the menu when clicking outside of it.
  useEffect(() => {
    const close = (e: MouseEvent) => {
      if (ref.current && !ref.current.contains(e.target as Node)) ref.current.open = false
    }
    document.addEventListener('click', close)
    return () => document.removeEventListener('click', close)
  }, [])

  return (
    <details ref={ref} className={styles.menu}>
      <summary className={styles.summary} aria-label="Select language">
        {current.label}
        <span className={styles.chevron} aria-hidden="true" />
      </summary>
      <ul className={styles.list}>
        {languages.map((l) => (
          <li key={l.code}>
            <Link
              href={localizedPath(l.code, path)}
              hrefLang={l.code}
              lang={l.code}
              className={styles.item}
              aria-current={l.code === lang ? 'page' : undefined}
            >
              {l.label}
            </Link>
          </li>
        ))}
      </ul>
    </details>
  )
}
