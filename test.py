import asyncio
import struct
import sys

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8085

# Структура: struct __attribute__((packed)) { uint32_t id; uint32_t size; }
# '<II' — Little-Endian, два uint32_t (по 4 байта каждый)
PACKET_FORMAT = '<II'
HEADER_SIZE = struct.calcsize(PACKET_FORMAT)

async def receive_handler(reader):
    """Задача: Постоянно читать данные от сервера строго по пакетам и выводить."""
    print("[Async] Запущено фоновое чтение от сервера...")
    try:
        while True:
            # Читаем заголовок строго по размеру
            try:
                header_data = await reader.readexactly(HEADER_SIZE)
            except asyncio.IncompleteReadError:
                print("\n[Async] Сервер закрыл соединение.")
                break
            
            packet_id, packet_size = struct.unpack(PACKET_FORMAT, header_data)
            print(f"[Отладка] Получен заголовок пакета. ID: {packet_id}, Ожидаемый размер тела: {packet_size} байт", file=sys.stderr)
            
            payload_data = b""
            if packet_size > 0:
                try:
                    # Ставим таймаут в 3 секунды, чтобы код не висел вечно, если сервер обманул с размером
                    payload_data = await asyncio.wait_for(
                        reader.readexactly(packet_size), 
                        timeout=3.0
                    )
                except asyncio.TimeoutError:
                    print(f"[ВНИМАНИЕ] Зависли на чтении тела пакета! Сервер обещал {packet_size} байт, но не прислал их вовремя.")
                    # Читаем то, что есть в буфере прямо сейчас, чтобы не ломать поток полностью
                    payload_data = await reader.read(packet_size)
                    print(f"[Отладка] Удалось вытянуть из буфера только: {len(payload_data)} байт", file=sys.stderr)

            text = payload_data.decode('utf-8', errors='replace')
            if text:
                print(text, end="") 
    except asyncio.IncompleteReadError:
        # readexactly кидает это исключение, если поток закончился посреди чтения
        print("\n[Async] Сервер закрыл соединение (поток прерван). Нажмите Ctrl+C для выхода.")
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
            
            if not user_text:
                continue
                
            # Переводим текст в байты
            payload = user_text.encode('utf-8')
            
            # Собираем заголовок пакета
            packet_id = 4
            packet_size = len(payload)
            header = struct.pack(PACKET_FORMAT, packet_id, packet_size)
            
            # Склеиваем заголовок и полезную нагрузку
            full_packet = header + payload
            
            # Отправляем в сокет
            writer.write(full_packet)
            await writer.drain() # Ждем, пока буфер сокета физически очистится
    except asyncio.CancelledError:
        pass
    except Exception as e:
        print(f"\n[Ошибка отправки]: {e}")

async def main():
    print(f"Подключение к {SERVER_IP}:{SERVER_PORT}...")
    try:
        # Открываем асинхронное TCP-соединение

        reader, writer = await asyncio.open_unix_connection("socket.sock")
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
