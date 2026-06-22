import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import re
import sys

# ================= CONFIGURAÇÕES =================
PORTA = 'COM5'  # <--- MUDE ISTO PARA A SUA PORTA
BAUD_RATE = 115200
MAX_PONTOS = 400  # Quantidade de amostras visíveis no ecrã ao mesmo tempo
# =================================================

# Tenta abrir a porta serial
try:
    ser = serial.Serial(PORTA, BAUD_RATE, timeout=0.01)
    print(f"Conectado à porta {PORTA} com sucesso!")
except Exception as e:
    print(f"Erro ao abrir a porta serial {PORTA}. Verifique se não está aberta noutro programa!")
    sys.exit()

# Listas para guardar os dados
tempos = []
valores_raw = []
valores_filtrados = []

# Configuração da janela do gráfico
fig, ax = plt.subplots(figsize=(10, 5))

# Linha para o sinal BRUTO (fina e um pouco transparente)
linha_raw, = ax.plot([], [], color='cyan', linestyle='-', linewidth=1, alpha=0.6, label='ADC Raw (com ruído)')

# Linha para o sinal FILTRADO (vermelha, mais grossa)
linha_filt, = ax.plot([], [], color='red', linestyle='-', linewidth=2, label='ADC Filtrado (FIR)')

ax.set_title("Comparação em Tempo Real: Sinal Bruto vs Filtro FIR", fontsize=14)
ax.set_xlabel("Tempo (ms)", fontsize=12)
ax.set_ylabel("Tensão (mV)", fontsize=12)
ax.set_ylim(-100, 3500) # O ADC vai de 0 a 3300mV
ax.grid(True, linestyle='--', alpha=0.7)
ax.legend(loc='upper right')

# Nova Expressão Regular: Procura L, seguido de 3 grupos de números separados por '#'
padrao_dados = re.compile(r"L(\d+)#(\d+)#(\d+)")

def atualizar_grafico(frame):
    # Lê TODAS as linhas que estão acumuladas no buffer do cabo USB
    while ser.in_waiting > 0:
        try:
            # Lê uma linha e remove espaços/quebras de linha
            linha_lida = ser.readline().decode('utf-8').strip()
            
            # Aplica o filtro Regex
            match = padrao_dados.search(linha_lida)
            if match:
                # Extrai os três valores da string
                tempo_ms = int(match.group(1))
                valor_raw = int(match.group(2))
                valor_filt = int(match.group(3))
                
                # Guarda nas listas
                tempos.append(tempo_ms)
                valores_raw.append(valor_raw)
                valores_filtrados.append(valor_filt)
                
        except Exception:
            # Ignora erros de decodificação se o cabo for tocado/ruído na serial
            pass 
            
    # Mantém apenas os últimos 'MAX_PONTOS' para o gráfico não ficar pesado
    tempos_plot = tempos[-MAX_PONTOS:]
    raw_plot = valores_raw[-MAX_PONTOS:]
    filt_plot = valores_filtrados[-MAX_PONTOS:]
    
    # Atualiza o desenho no ecrã
    if tempos_plot:
        linha_raw.set_data(tempos_plot, raw_plot)
        linha_filt.set_data(tempos_plot, filt_plot)
        
        # Faz o eixo X "andar" junto com o tempo
        ax.set_xlim(min(tempos_plot), max(tempos_plot) + 10)
        
    # Precisamos retornar ambas as linhas desenhadas
    return linha_raw, linha_filt

print("A aguardar dados... (Feche a janela do gráfico para sair)")

# Cria a animação que chama a função 'atualizar_grafico' a cada 20 milissegundos
ani = animation.FuncAnimation(fig, atualizar_grafico, interval=20, blit=False, cache_frame_data=False)

# Mostra a janela nativa do matplotlib
plt.tight_layout()
plt.show()

# Quando a janela for fechada, fecha a porta serial
ser.close()
print("Conexão encerrada.")