#include <stdio.h>
#include <stdlib.h>
#include "xtft.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xintc.h"
#include "start.h"
#include "bkg.h"
#include "grass.h"
#define TFT_DEVICE_ID XPAR_TFT_0_DEVICE_ID
#define TFT_FRAME_ADDR0 XPAR_EMC_0_MEM0_HIGHADDR-0x001FFFFF   //定义显示存储器的基地址
#define FGCOLOR_grn 0x1c     //绿色
#define FGCOLOR_blu 0x3      //蓝色
#define FGCOLOR_red 0xe0     //红色
#define COLOR_BLACK 0x00     //黑色
#define COLOR_WRITE 0xff     //白色
static XTft TftInstance;     //实例化tft
XTft_Config *TftConfigPtr;
XIntc intCtrl;               //实例化中断控制器
XGpio Btns,Dips;             //实例化GPIO
#define SCREEN_WIDTH  35   /*游戏屏幕宽度，和实际显示大小成比例*/
#define SCREEN_LENGTH 25   /*游戏屏幕长度*/
#define START_X 2         /*屏幕起始X坐标*/
#define START_Y 2          /*屏幕起始y坐标*/
#define ADD_NUM 16          //游戏画面和显示屏幕大小比，即为蛇身一块的大小，即16*16
int count = 10;           //作为随机数生成的初始种子
void Initialize();
void Delay();
void drawwall(void);
int TftWriteString(XTft *InstancePtr, const u8 *CharValue);
void PushBtnHandler(void *CallBackRef);    //按键的中断控制函数
void SwithHandler(void *CallBackRef);
int pshBtn = 0;                                //按键有无按下的标志位
int state1;                                //按键状态
int state2;   //开关状态
int pauseflag=0;
int isfuhuo=0;
int score=0;
/*----------------------------------硬件相关初始化及设置------------------------------------------*/

void Initialize()
{
	//初始化Btns实例，并设定其为输入方式
	XGpio_Initialize(&Btns,XPAR_BUTTON_DEVICE_ID);
	XGpio_SetDataDirection(&Btns,1,0xff);
	//初始化Dips实例，并设定其为输入方式
	XGpio_Initialize(&Dips,XPAR_DIP_DEVICE_ID);
	XGpio_SetDataDirection(&Dips,1,0xff);
	//初始化IntCtrl实例
	XIntc_Initialize(&intCtrl,XPAR_AXI_INTC_0_DEVICE_ID);
	//GPIO中断使能
	XGpio_InterruptEnable(&Btns,1);
	XGpio_InterruptGlobalEnable(&Btns);
	XGpio_InterruptEnable(&Dips,1);
	XGpio_InterruptGlobalEnable(&Dips);
	//对中断控制器进行中断源使能
	XIntc_Enable(&intCtrl,XPAR_AXI_INTC_0_BUTTON_IP2INTC_IRPT_INTR);
	XIntc_Enable(&intCtrl,XPAR_AXI_INTC_0_DIP_IP2INTC_IRPT_INTR);
	//注册中断服务函数
	XIntc_Connect(&intCtrl,XPAR_AXI_INTC_0_BUTTON_IP2INTC_IRPT_INTR,
			(XInterruptHandler)PushBtnHandler,(void *)0);
	XIntc_Connect(&intCtrl,XPAR_AXI_INTC_0_DIP_IP2INTC_IRPT_INTR,
			(XInterruptHandler)SwithHandler,(void *)1);
	microblaze_enable_interrupts();  //允许处理器处理中断
	//注册中断控制器处理函数
	microblaze_register_handler((XInterruptHandler)XIntc_InterruptHandler,(void *)&intCtrl);
	XIntc_Start(&intCtrl,XIN_REAL_MODE);  //启动中断控制器
	TftConfigPtr=XTft_LookupConfig(TFT_DEVICE_ID);
	XTft_CfgInitialize(&TftInstance,TftConfigPtr,TftConfigPtr->BaseAddress);  //初始化tft
	XTft_SetFrameBaseAddr(&TftInstance,TFT_FRAME_ADDR0);                      //设置tft的存储基地址
	XTft_ClearScreen(&TftInstance);
}
void Delay()
{
	int i;
	for(i=0;i<30000;i++);
}
void PushBtnHandler(void *CallBackRef)
{
	state1 = XGpio_DiscreteRead(&Btns,1);  //读取按键的状态值
	pshBtn = 1;
	XGpio_InterruptDisable(&Btns,1);  //暂时禁止button中断
	Delay();  //延时，忽略按键弹起再次触发的中断
	XGpio_InterruptClear(&Btns,1);  //清除中断标志位
	XGpio_InterruptEnable(&Btns,1);  //再次开放按键中断
}

void SwithHandler(void *CallBackRef)
{
	state2 = XGpio_DiscreteRead(&Dips,1);
	if(state2==0b00000000)pauseflag=1;
	XGpio_InterruptClear(&Dips,1);

}

/*---------------------------------贪吃蛇的主要逻辑部分及显示相关------------------------------------*/

enum direc{ up, down, left, right };  /*蛇的运动方向*/

typedef struct snake{               /*蛇身结构体，是蛇的某一部分*/
	int x;                      //横坐标
	int y;                      //纵坐标
	struct snake *next;         //队列的下一个
	struct snake *pre;          //队列的前一个
	struct snake *end;          //队列的尾端
}SNAKE;

typedef struct{                /*食物*/
	int x;
	int y;
}FOOD;

int Random(int n)          /*随机生成食物的位置*/
{
	srand(count++);
	return (rand() % n);
}

void BuildSnk(SNAKE *head)  /*画出蛇身*/
{
	SNAKE *p = head;
	while (p != NULL)
	{
		XTft_FillScreen(&TftInstance,(p->x)*ADD_NUM,(p->y)*ADD_NUM,(p->x + 1)*ADD_NUM - 1,
				(p->y + 1)*ADD_NUM - 1,FGCOLOR_grn);        //通过队列索引画出蛇的组成一部分
		p = p->next;

	}
}

void RemoveSnk(SNAKE *head)   /*清除函数*/
{
	SNAKE *p = head;
	while (p != NULL)
	{
		int starty,startx,endy,endx,i,j;
		starty=(p->y)*ADD_NUM;
		startx=(p->x)*ADD_NUM;
		endy=(p->y + 1)*ADD_NUM ;
		endx=(p->x + 1)*ADD_NUM ;
//		XTft_FillScreen(&TftInstance,(p->x)*ADD_NUM,(p->y)*ADD_NUM,(p->x + 1)*ADD_NUM - 1,
//				(p->y + 1)*ADD_NUM - 1,COLOR_BLACK );        //画出蛇的组成一部分
		for (j=starty;j<endy;j++)//120 300
		{
			for(i=startx;i<endx;i++)//220 460
			{
				XTft_SetPixel(&TftInstance,i,j,gImage_grass[j*16+i]);
			}
		}
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
		free(p);             //释放蛇的内存
		p = p_temp;
	}
}

void Move(int *d)  //对中断接受到的键值进行判断，1为上，8为下，2为左，4为右
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
		pshBtn = 0;         //标志位置0说明有键按下
	}
}

void IniScreen(SNAKE *head)   /*初始化屏幕*/
{
	int i;
	SNAKE *p1, *p2;
	drawwall();
	/*--------------将蛇初始化，对链表进行一系列设置-------------------*/
	head->x = START_X + SCREEN_WIDTH / 2;
	head->y = START_Y + SCREEN_LENGTH / 2;       //初始位置为屏幕中间
	head->pre = NULL;
	p1 = head;
	i = 0;
	while (++i < 3)                 //通过循环建立三个snake数据结构并相连
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
	BuildSnk(head);           //显示蛇身
}

void CreatFood(FOOD *fd, SNAKE *snk)  /*生成食物，并且让食物不与蛇体冲突*/
{
	SNAKE *p = snk;
	int clash = 0;    /*标记食物位置是否和蛇发生冲突*/
	while (1)
	{
		clash = 0;
		fd->x = START_X + 1 + Random(SCREEN_WIDTH-2);  /* x控制在边宽内 */
		fd->y = START_Y + 1 + Random(SCREEN_LENGTH-2);  /* y控制在变长内 */
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
			(fd->y + 1)*ADD_NUM - 1,FGCOLOR_red);         //显示食物
}

int Eated(SNAKE *head, FOOD *fd)            /*有没有吃到食物*/
{
	if (head->x == fd->x && head->y == fd->y)
		return 1;
	return 0;
}

int GameOver(SNAKE *head)      /*判断游戏结束了没.*/
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

	char buffer[10]="Score=";
	//XTft_DisableDisplay(&TftInstance);
//	for (j=120;j<120+100;j++)//120 300
//	{
//		for(i=220;i<220+100;i++)//220 460
//		{
//			XTft_SetPixel(&TftInstance,i,j,gImage_gameover[(j-120)*100+i-220]);
//		}
//	}
	XTft_FillScreen(&TftInstance,0,0,
						640-1, 480-1,0xc9);
	isfuhuo=1;
	XTft_SetPosChar(&TftInstance, 260, 230);
	TftWriteString(&TftInstance,"Game End!!!");
	XTft_SetPosChar(&TftInstance, 280, 250);
	TftWriteString(&TftInstance,buffer);
	XTft_Write(&TftInstance,'0'+score);
	while(!pshBtn);
	score=0;
	//XTft_EnableDisplay(&TftInstance);
}

void gaming()   /*游戏过程*/
{
	int eat = 0;
	int direct = up;   /*  初始方向为向上 */
	FOOD *fd;
	SNAKE *head, *ptemp;  /*最少3个蛇身 + 1个蛇头的时候才会挂*/

	head = (SNAKE *)malloc(sizeof(SNAKE));
	fd = (FOOD *)malloc(sizeof(FOOD));
	pshBtn = 0;
	XTft_ClearScreen(&TftInstance);
	IniScreen(head);
	CreatFood(fd, head);
	while (1)
	{

		if(!pauseflag)    /*控制暂停*/
		{
		RemoveSnk(head);
		ptemp = (SNAKE *)malloc(sizeof(SNAKE));   /*临时的，用来增加在蛇的头部*/
		Move(&direct);   /*....接受用户选择蛇的运动方向*/
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

		if (GameOver(head) || (head->x == START_X) || (head->x == START_X + SCREEN_WIDTH )
				|| (head->y == START_Y) || (head->y == START_Y + SCREEN_LENGTH ))   //游戏是否结束
		{
//			BuildSnk(head);
			XTft_ClearScreen(&TftInstance);
			DeleteSnack(head);        //别忘了将内存释放掉
			free(fd);
			GameOver_Show();
			return;
		}
		if (!eat)    /*如果没吃到，那自然增加蛇的长度*/
		{
			ptemp->end = head->end->pre;   //将蛇的尾端位置改变，变为倒数第二个
			head->end->pre->next = NULL;  //再连接下
			free(head->end);
			head->end = NULL;
		}
		else
		{
			ptemp->end = head->end;/*吃到了。。在原有的基础上换一个头，这样就多了一节*/
		}
		head->pre = ptemp;          //链表连接
		ptemp->next = head;
		ptemp->pre = NULL;
		head = ptemp;
		if (eat = Eated(head, fd))
		{
			CreatFood(fd, head);
			score++;
		}
		BuildSnk(head);
		int i, j;
		for (i = 0; i < 2500-score*180 ;i++)
			for (j = 0; j < 1000; j++);    //延时可控制蛇的速度
		}else{
			XTft_ClearScreen(&TftInstance);
			XTft_FillScreen(&TftInstance,0,0,
								640-1, 480-1,FGCOLOR_blu);
			XTft_SetPosChar(&TftInstance, 240, 230);
			TftWriteString(&TftInstance,"Game Pause!!!");
			while(state2==0b00000000);
			pauseflag=0;
			XTft_ClearScreen(&TftInstance);
			drawwall();
			BuildSnk(head);
			CreatFood(fd, head);

		}

	}
}

int main()
{
	Initialize();

	while (1)
	{
		if(!isfuhuo)
		{
			XTft_FillScreen(&TftInstance,0,0,
								640-1, 480-1,COLOR_WRITE);
			int i,j;
			for (j=100;j<292+100;j++)//120 300
			{
				for(i=50;i<517+50;i++)//220 460
				{
					XTft_SetPixel(&TftInstance,i,j,gImage_start[(j-100)*517+i-50]);
				}
			}
			XTft_SetPosChar(&TftInstance, 240, 240);
			TftWriteString(&TftInstance,"Press Button to start the Game!!!");
			while(!pshBtn);
		}

		gaming();
	}
	return 0;
}

int TftWriteString(XTft *InstancePtr, const u8 *CharValue)
{
	while (*CharValue != 0) {
		XTft_Write(InstancePtr, *CharValue);
		CharValue++;
	}
	return XST_SUCCESS;
}
void drawwall(void)
{
	int i,j;


//	XTft_FillScreen(&TftInstance,0,0,
//						640-1, 16*3-1,COLOR_WRITE);
//	XTft_FillScreen(&TftInstance,0,480-16*3,
//						640-1, 480-1,COLOR_WRITE);
//	XTft_FillScreen(&TftInstance,0,0,
//						16*3-1, 480-1,COLOR_WRITE);
//	XTft_FillScreen(&TftInstance,640-16*3,0,
//						640-1, 480-1,COLOR_WRITE);
	for (j=0;j<48;j++)//120 300
	{
		for(i=0;i<160*4;i++)//220 460
		{
			XTft_SetPixel(&TftInstance,i,j,gImage_wall[j*160+i%160]);
		}
	}
	for (j=480-48;j<480;j++)//120 300
	{
		for(i=0;i<160*4;i++)//220 460
		{
			XTft_SetPixel(&TftInstance,i,j,gImage_wall[(j-480+48)*160+i%160]);
		}
	}
	for (j=0;j<480;j++)//120 300
	{
		for(i=0;i<48;i++)//220 460
		{
			XTft_SetPixel(&TftInstance,i,j,gImage_wall[j%160*160+i]);
		}
		for(i=640-48;i<640;i++)//220 460
		{
			XTft_SetPixel(&TftInstance,i,j,gImage_wall[j%160*160+i-640+48]);
		}
	}
	for (j=48;j<480-48;j++)//120 300
	{
		for(i=48;i<640-48;i++)//220 460
		{
			XTft_SetPixel(&TftInstance,i,j,gImage_grass[(j-48)*592+i-48]);
		}
	}

}

