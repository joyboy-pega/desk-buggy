import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'Desk Buggy Studio — Web Serial Flasher & 1:1 Hardware Simulator',
  description: 'Push firmware directly to ESP32 and ESP32-C3 hardware over Web Serial, with 1:1 pixel-perfect interactive OLED and TFT screen replicas.',
  icons: {
    icon: '/favicon.ico',
  },
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="antialiased bg-[#070709] text-gray-100 min-h-screen">
        {children}
      </body>
    </html>
  );
}
