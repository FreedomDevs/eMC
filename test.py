import asyncio
import struct
import sys

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8085

# Структура: struct __attribute__((packed)) { uint32_t id; uint32_t size; }
# '<II' — Little-Endian, два uint32_t (по 4 байта каждый)
PACKET_FORMAT = '<II'

async def receive_handler(reader):
    """Задача: Постоянно читать данные от сервера и красиво выводить."""
    print("[Async] Запущено фоновое чтение от сервера...")
    try:
        while True:
            # Читаем порцию данных (до 4096 байт) асинхронно
            data = await reader.read(4096)
            
            if not data:
                print("\n[Async] Сервер закрыл соединение. Нажмите Ctrl+C для выхода.")
                break
            
            # Декодируем и выводим в нужном тебе формате
            text = data.decode('utf-8', errors='replace').strip()
            if text:
                print(f"\n[Сервер] Тип 1: <{text}>")
                # Печатаем приглашение заново, так как сервер мог перебить наш ввод
                print("Введите текст для отправки: ", end='', flush=True)

    except asyncio.CancelledError:
        pass
    except Exception as e:
        print(f"\n[Ошибка чтения]: {e}")

async def send_handler(writer):
    # Чтобы input() не блокировал весь асинхронный цикл, 
    # вынесем чтение stdin в отдельный поток через run_in_executor
    loop = asyncio.get_event_loop()
    
    try:
        payload = b""
        packet_id = 3
        packet_size = len(payload)
        header = struct.pack(PACKET_FORMAT, packet_id, packet_size)
        writer.write(header + payload)
        await writer.drain()

        while True:
            # Асинхронно ждем ввод пользователя
            user_text = await loop.run_in_executor(None, sys.stdin.readline)
            user_text = user_text.strip()
            
            if not user_text:
                continue
                
            # Переводим текст в байты
            payload = user_text.encode('utf-8')
            
            # Собираем заголовок пакета
            packet_id = 1
            packet_size = len(payload)
            header = struct.pack(PACKET_FORMAT, packet_id, packet_size)
            
            # Склеиваем заголовок и полезную нагрузку
            full_packet = header + payload
            
            # Отправляем в сокет
            writer.write(full_packet)
            await writer.drain() # Ждем, пока буфер сокета физически очистится
            print(f"[Успешно отправлено {len(full_packet)} байт (id={packet_id}, size={packet_size})]")

    except asyncio.CancelledError:
        pass
    except Exception as e:
        print(f"\n[Ошибка отправки]: {e}")

async def main():
    print(f"Подключение к {SERVER_IP}:{SERVER_PORT}...")
    try:
        # Открываем асинхронное TCP-соединение
        reader, writer = await asyncio.open_connection(SERVER_IP, SERVER_PORT)
        print("Подключено!")
    except Exception as e:
        print(f"Ошибка подключения: {e}")
        return

    # Запускаем чтение и запись параллельно
    tasks = asyncio.gather(
        receive_handler(reader),
        send_handler(writer)
    )
    
    try:
        await tasks
    except KeyboardInterrupt:
        print("\nВыход по Ctrl+C...")
    finally:
        writer.close()
        await writer.wait_closed()
        print("Соединение закрыто.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
