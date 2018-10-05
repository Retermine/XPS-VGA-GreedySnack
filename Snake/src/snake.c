#include <stdio.h>
#include <stdlib.h>
#include "xtft.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xintc.h"
#include "gameover.h"
#define TFT_DEVICE_ID XPAR_TFT_0_DEVICE_ID
#define TFT_FRAME_ADDR0 XPAR_EMC_0_MEM0_HIGHADDR-0x001FFFFF   //������ʾ�洢���Ļ���ַ
#define FGCOLOR_grn 0x1c     //��ɫ
#define FGCOLOR_blu 0x3      //��ɫ
#define FGCOLOR_red 0xe0     //��ɫ
#define COLOR_BLACK 0x00     //��ɫ
#define COLOR_WRITE 0xff     //��ɫ
static XTft TftInstance;     //ʵ����tft
XTft_Config *TftConfigPtr;
XIntc intCtrl;               //ʵ�����жϿ�����
XGpio Btns,Dips;             //ʵ����GPIO
#define SCREEN_WIDTH  39   /*��Ϸ��Ļ��ȣ���ʵ����ʾ��С�ɱ���*/
#define SCREEN_LENGTH 29   /*��Ϸ��Ļ����*/
#define START_X 0         /*��Ļ��ʼX����*/
#define START_Y 0          /*��Ļ��ʼy����*/
#define ADD_NUM 16          //��Ϸ�������ʾ��Ļ��С�ȣ���Ϊ����һ��Ĵ�С����16*16
int count = 10;           //��Ϊ��������ɵĳ�ʼ����
void Initialize();
void Delay();
void PushBtnHandler(void *CallBackRef);    //�������жϿ��ƺ���
void SwithHandler(void *CallBackRef);
int pshBtn = 0;                                //�������ް��µı�־λ
int state1;                                //����״̬
int state2;                                //����״̬

/*----------------------------------Ӳ����س�ʼ��������------------------------------------------*/

void Initialize()
{
	//��ʼ��Btnsʵ�������趨��Ϊ���뷽ʽ
	XGpio_Initialize(&Btns,XPAR_BUTTON_DEVICE_ID);
	XGpio_SetDataDirection(&Btns,1,0xff);
	//��ʼ��Dipsʵ�������趨��Ϊ���뷽ʽ
	XGpio_Initialize(&Dips,XPAR_DIP_DEVICE_ID);
	XGpio_SetDataDirection(&Dips,1,0xff);
	//��ʼ��IntCtrlʵ��
	XIntc_Initialize(&intCtrl,XPAR_AXI_INTC_0_DEVICE_ID);
	//GPIO�ж�ʹ��
	XGpio_InterruptEnable(&Btns,1);
	XGpio_InterruptGlobalEnable(&Btns);
	XGpio_InterruptEnable(&Dips,1);
	XGpio_InterruptGlobalEnable(&Dips);
	//���жϿ����������ж�Դʹ��
	XIntc_Enable(&intCtrl,XPAR_AXI_INTC_0_BUTTON_IP2INTC_IRPT_INTR);
	XIntc_Enable(&intCtrl,XPAR_AXI_INTC_0_DIP_IP2INTC_IRPT_INTR);
	//ע���жϷ�����
	XIntc_Connect(&intCtrl,XPAR_AXI_INTC_0_BUTTON_IP2INTC_IRPT_INTR,
			(XInterruptHandler)PushBtnHandler,(void *)0);
	XIntc_Connect(&intCtrl,XPAR_AXI_INTC_0_DIP_IP2INTC_IRPT_INTR,
			(XInterruptHandler)SwithHandler,(void *)1);
	microblaze_enable_interrupts();  //�������������ж�
	//ע���жϿ�����������
	microblaze_register_handler((XInterruptHandler)XIntc_InterruptHandler,(void *)&intCtrl);
	XIntc_Start(&intCtrl,XIN_REAL_MODE);  //�����жϿ�����
	TftConfigPtr=XTft_LookupConfig(TFT_DEVICE_ID);
	XTft_CfgInitialize(&TftInstance,TftConfigPtr,TftConfigPtr->BaseAddress);  //��ʼ��tft
	XTft_SetFrameBaseAddr(&TftInstance,TFT_FRAME_ADDR0);                      //����tft�Ĵ洢����ַ
}
void Delay()
{
	int i;
	for(i=0;i<30000;i++);
}
void PushBtnHandler(void *CallBackRef)
{
	state1 = XGpio_DiscreteRead(&Btns,1);  //��ȡ������״ֵ̬
	pshBtn = 1;
	XGpio_InterruptDisable(&Btns,1);  //��ʱ��ֹbutton�ж�
	Delay();  //��ʱ�����԰��������ٴδ������ж�
	XGpio_InterruptClear(&Btns,1);  //����жϱ�־λ
	XGpio_InterruptEnable(&Btns,1);  //�ٴο��Ű����ж�
}

void SwithHandler(void *CallBackRef)
{
	state2 = XGpio_DiscreteRead(&Dips,1);
	XGpio_InterruptClear(&Dips,1);
}

/*---------------------------------̰���ߵ���Ҫ�߼����ּ���ʾ���------------------------------------*/

enum direc{ up, down, left, right };  /*�ߵ��˶�����*/

typedef struct snake{               /*����ṹ�壬���ߵ�ĳһ����*/
	int x;                      //������
	int y;                      //������
	struct snake *next;         //���е���һ��
	struct snake *pre;          //���е�ǰһ��
	struct snake *end;          //���е�β��
}SNAKE;

typedef struct{                /*ʳ��*/
	int x;
	int y;
}FOOD;

int Random(int n)          /*�������ʳ���λ��*/
{
	srand(count++);
	return (rand() % n);
}

void BuildSnk(SNAKE *head)  /*��������*/
{
	SNAKE *p = head;
	while (p != NULL)
	{
		XTft_FillScreen(&TftInstance,(p->x)*ADD_NUM,(p->y)*ADD_NUM,(p->x + 1)*ADD_NUM - 1,
				(p->y + 1)*ADD_NUM - 1,FGCOLOR_blu);        //ͨ���������������ߵ����һ����
		p = p->next;

	}
}

void RemoveSnk(SNAKE *head)   /*�������*/
{
	SNAKE *p = head;
	while (p != NULL)
	{
		XTft_FillScreen(&TftInstance,(p->x)*ADD_NUM,(p->y)*ADD_NUM,(p->x + 1)*ADD_NUM - 1,
				(p->y + 1)*ADD_NUM - 1,COLOR_BLACK );        //�����ߵ����һ����
		p = p->next;
	}
}

void DeleteSnack(SNAKE *head)
{
	SNAKE *p = head -> end;
	SNAKE *p_temp;
	while (p != NULL)
	{
		p_temp = p -> pre;
		free(p);             //�ͷ��ߵ��ڴ�
		p = p_temp;
	}
}

void Move(int *d)  //���жϽ��ܵ��ļ�ֵ�����жϣ�1Ϊ�ϣ�8Ϊ�£�2Ϊ��4Ϊ��
{
	while (pshBtn)
	{
		if (state1 == 1)
		{
			if ((*d == left || *d == right) && *d != down)
				*d = up;
		}
		else if (state1 == 8)
		{
			if ((*d == left || *d == right) && *d != up)
				*d = down;
		}
		else if (state1 == 2)
		{
			if ((*d == up || *d == down) && *d != right)
				*d = left;
		}
		else if (state1 == 4)
		{
			if ((*d == up || *d == down) && *d != left)
				*d = right;
		}
		pshBtn = 0;         //��־λ��0˵���м�����
	}
}

void IniScreen(SNAKE *head)   /*��ʼ����Ļ*/
{
	int i;
	SNAKE *p1, *p2;

	/*--------------���߳�ʼ�������������һϵ������-------------------*/
	head->x = START_X + SCREEN_WIDTH / 2;
	head->y = START_Y + SCREEN_LENGTH / 2;       //��ʼλ��Ϊ��Ļ�м�
	head->pre = NULL;
	p1 = head;
	i = 0;
	while (++i < 3)                 //ͨ��ѭ����������snake���ݽṹ������
	{

		p2 = p1;
		p1 = (SNAKE *)malloc(sizeof(SNAKE));
		p1->x = START_X + SCREEN_WIDTH / 2;
		p1->y = START_Y + SCREEN_LENGTH / 2 + i;
		p1->end = NULL;
		p2->next = p1;
		p1->pre = p2;
	}
	p1->next = NULL;
	head->end = p1;
	BuildSnk(head);           //��ʾ����
}

void CreatFood(FOOD *fd, SNAKE *snk)  /*����ʳ�������ʳ�ﲻ�������ͻ*/
{
	SNAKE *p = snk;
	int clash = 0;    /*���ʳ��λ���Ƿ���߷�����ͻ*/
	while (1)
	{
		clash = 0;
		fd->x = START_X + 1 + Random(SCREEN_WIDTH);  /* x�����ڱ߿��� */
		fd->y = START_Y + 1 + Random(SCREEN_LENGTH);  /* y�����ڱ䳤�� */
		for (; p != NULL; p = p->next)
		if (fd->x == p->x && fd->y == p->y)
		{
			clash = 1;
			break;
		}
		if (clash == 0)
			break;
	}
	XTft_FillScreen(&TftInstance,(fd->x)*ADD_NUM,(fd->y)*ADD_NUM,(fd->x + 1)*ADD_NUM - 1,
			(fd->y + 1)*ADD_NUM - 1,FGCOLOR_red);         //��ʾʳ��
}

int Eated(SNAKE *head, FOOD *fd)            /*��û�гԵ�ʳ��*/
{
	if (head->x == fd->x && head->y == fd->y)
		return 1;
	return 0;
}

int GameOver(SNAKE *head)      /*�ж���Ϸ������û.*/
{
	SNAKE *p;
	for (p = head->next; p != NULL; p = p->next)
	{
		if (head->x == p->x && head->y == p->y)
			return 1;
	}
	return 0;
}

void GameOver_Show()
{
	int i,j;
	//XTft_DisableDisplay(&TftInstance);
	for (j=120;j<300;j++)
	{
		for(i=220;i<460;i++)
		{
			XTft_SetPixel(&TftInstance,i,j,gImage_gameover[(j-120)*240+i-220]);
		}
	}
	//XTft_EnableDisplay(&TftInstance);
}

void gaming()   /*��Ϸ����*/
{
	int eat = 0;
	int direct = up;   /*  ��ʼ����Ϊ���� */
	FOOD *fd;
	SNAKE *head, *ptemp;  /*����3������ + 1����ͷ��ʱ��Ż��*/

	head = (SNAKE *)malloc(sizeof(SNAKE));
	fd = (FOOD *)malloc(sizeof(FOOD));
	while (!pshBtn);      //���ܰ�����ʼ��Ϸ
	pshBtn = 0;
	XTft_ClearScreen(&TftInstance);
	IniScreen(head);
	CreatFood(fd, head);
	while (1)
	{
		if(state2!=0b00000000)    /*������ͣ*/
		{
		RemoveSnk(head);
		ptemp = (SNAKE *)malloc(sizeof(SNAKE));   /*��ʱ�ģ������������ߵ�ͷ��*/
		Move(&direct);   /*....�����û�ѡ���ߵ��˶�����*/
		switch (direct)
		{
		case up: ptemp->x = head->x;
			ptemp->y = head->y - 1;
			break;
		case down: ptemp->x = head->x;
			ptemp->y = head->y + 1;
			break;
		case left: ptemp->x = head->x - 1;
			ptemp->y = head->y;
			break;
		case right: ptemp->x = head->x + 1;
			ptemp->y = head->y;
			break;
		}
		if (!eat)    /*���û�Ե�������Ȼ�����ߵĳ���*/
		{
			ptemp->end = head->end->pre;   //���ߵ�β��λ�øı䣬��Ϊ�����ڶ���
			head->end->pre->next = NULL;  //��������
			free(head->end);
			head->end = NULL;
		}
		else
		{
			ptemp->end = head->end;/*�Ե��ˡ�����ԭ�еĻ����ϻ�һ��ͷ�������Ͷ���һ��*/
		}
		head->pre = ptemp;          //��������
		ptemp->next = head;
		ptemp->pre = NULL;
		head = ptemp;
		if (eat = Eated(head, fd))
		{
			CreatFood(fd, head);
		}
		if (GameOver(head) || (head->x == START_X) || (head->x == START_X + SCREEN_WIDTH )
				|| (head->y == START_Y) || (head->y == START_Y + SCREEN_LENGTH ))   //��Ϸ�Ƿ����
		{
			BuildSnk(head);
			XTft_ClearScreen(&TftInstance);
			DeleteSnack(head);        //�����˽��ڴ��ͷŵ�
			free(fd);
			GameOver_Show();
			return;
		}
		BuildSnk(head);
		int i, j;
		for (i = 0; i < 3000-12*state2 ;i++)
			for (j = 0; j < 1000; j++);    //��ʱ�ɿ����ߵ��ٶ�
		}
	}
}

int main()
{
	Initialize();
	while (1)
	{
		gaming();
	}
	return 0;
}
