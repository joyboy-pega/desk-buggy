/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./src/pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/components/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/app/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        nothing: {
          red: '#E01A22',
          black: '#000000',
          dark: '#0c0c0e',
          card: '#141416',
          border: '#242428',
          muted: '#7C7C82',
          glyph: '#38383E',
        },
        oled: {
          cyan: '#00F0FF',
          yellow: '#FFDF00',
          white: '#FFFFFF',
        }
      },
      fontFamily: {
        mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'Consolas', 'monospace'],
      }
    },
  },
  plugins: [],
};
