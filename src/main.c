#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <cv.h>
#include <cvaux.h>
#include <highgui.h>
#include "simpatica.h"

#define Width		320
#define Height		240
#define WidthOclusao	50
#define HeightOclusao	240
#define N		70
#define iteracoes	20

typedef struct __particula{
	int pos_atual[2];	//x|y
	double valor;

	double velocidade[2];	//x|y

	int melhor_pos[2];	//x|y
	double melhor_valor;
} Particula;


int tamanho, xInit, yInit, xEnd, yEnd, xOclusao, nPixels=0, random_seed;
double K, c1, c2, gama[2], lambda[2];

Particula swarm[N];
int melhor_pos_swarm[2];
double melhor_valor_swarm;

IplImage * cria_objeto_aleatorio(){
	unsigned long seed;
	int raio, i, j;
	double sigma=7.0f;
	gsl_rng *r;

	r=gsl_rng_alloc(gsl_rng_mt19937);
	srand(time(NULL));
	seed=rand();
	gsl_rng_set(r, seed);

	tamanho=55+round(gsl_ran_gaussian(r, sigma));
	if(tamanho%2==0)
		tamanho++;

	raio=tamanho/2;
	//gera posição inicial
	while(1){
		xInit=round((Width-1)*gsl_rng_uniform(r));
		if(xInit-raio>0 && xInit+raio<Width)
			break;
	}
	while(1){
		yInit=round((Height-1)*gsl_rng_uniform(r));
		if(yInit-raio>0 && yInit+raio<Height)
			break;
	}

	//gera posicao da oclusao
	while(1){
		xOclusao=round((Width-1)*gsl_rng_uniform(r));
		if(xInit+raio<xOclusao-WidthOclusao/2 || xInit-raio>xOclusao+WidthOclusao/2)
			if(xOclusao>WidthOclusao/2+tamanho && xOclusao<Width-WidthOclusao/2-tamanho)
				break;
	}

	//gera posição final
	while(1){
		xEnd=round((Width-1)*gsl_rng_uniform(r));
		if(xEnd+raio<xOclusao-WidthOclusao/2 || xEnd-raio>xOclusao+WidthOclusao/2)
			if((xInit<xOclusao && xEnd>xOclusao) || (xInit>xOclusao && xEnd<xOclusao))
				if(xEnd-raio>=0 && xEnd+raio<Width)
					break;
	}
	while(1){
		yEnd=round((Height-1)*gsl_rng_uniform(r));
		if(yEnd-raio>=0 && yEnd+raio<Height)
			break;
	}
	printf("tamanho: %d\nxInit: %d\nyInit: %d\n", tamanho, xInit, yInit);
	printf("xEnd: %d\nyEnd: %d\n", xEnd, yEnd);
	printf("xOclusao: %d\n", xOclusao);
}

void trajetoria(){
	IplImage *img;
	int xAtual, yAtual, raio;
	int i, j;
	CvVideoWriter *writer = 0;
	int isColor=1;
	int fps=5;

	writer=cvCreateVideoWriter("video.avi",CV_FOURCC('D', 'I', 'V','X'),fps,cvSize(Width,Height),isColor);

	xAtual=xInit;yAtual=yInit;
	while(xAtual!=xEnd || yAtual!=yEnd){
		img=cvCreateImage(cvSize(Width, Height), IPL_DEPTH_8U, 3);

		for(i=0;i<img->height;i++)
			for(j=0;j<img->width;j++){
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+0)=0;
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+1)=0;
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+2)=0;
			}

		//linha central
		raio=tamanho/2;
		for(j=xAtual-raio;j<xAtual+raio+1;j++)
			CV_IMAGE_ELEM(img, uchar, yAtual, j*img->nChannels+1)=255;

		//linhas superiores
		i=yAtual-1;
		raio=tamanho/2;
		raio-=1;
		while(raio>=0){
			for(j=xAtual-raio;j<xAtual+raio+1;j++)
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+1)=255;
			raio-=1;
			i--;
		}

		//linhas inferiores
		i=yAtual+1;
		raio=tamanho/2;
		raio-=1;
		while(raio>=0){
			for(j=xAtual-raio;j<xAtual+raio+1;j++)
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+1)=255;
			raio-=1;
			i++;
		}
		//oclusao
		for(i=0;i<HeightOclusao;i++)
			for(j=xOclusao-WidthOclusao/2;j<xOclusao+WidthOclusao/2;j++){
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+0)=255;
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+1)=255;
				CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+2)=255;
			}

		cvWriteFrame(writer,img);

		if(xAtual!=xEnd){
			if(xAtual>xEnd){
				if(xAtual-10>=xEnd)
					xAtual-=10;
				else if(xAtual-9>=xEnd)
					xAtual-=9;
				else if(xAtual-8>=xEnd)
					xAtual-=8;
				else if(xAtual-7>=xEnd)
					xAtual-=7;
				else if(xAtual-6>=xEnd)
					xAtual-=6;
				else if(xAtual-5>=xEnd)
					xAtual-=5;
				else if(xAtual-4>=xEnd)
					xAtual-=4;
				else if(xAtual-3>=xEnd)
					xAtual-=3;
				else if(xAtual-2>=xEnd)
					xAtual-=2;
				else if(xAtual-1>=xEnd)
					xAtual--;
			} else{
				if(xAtual+10<=xEnd)
					xAtual+=10;
				else if(xAtual+9<=xEnd)
					xAtual+=9;
				else if(xAtual+8<=xEnd)
					xAtual+=8;
				else if(xAtual+7<=xEnd)
					xAtual+=7;
				else if(xAtual+6<=xEnd)
					xAtual+=6;
				else if(xAtual+5<=xEnd)
					xAtual+=5;
				else if(xAtual+4<=xEnd)
					xAtual+=4;
				else if(xAtual+3<=xEnd)
					xAtual+=3;
				else if(xAtual+2<=xEnd)
					xAtual+=2;
				else if(xAtual+1<=xEnd)
					xAtual++;
			}
		}
		if(yAtual!=yEnd){
			if(yAtual>yEnd){
				if(yAtual-10>=yEnd)
					yAtual-=10;
				else if(yAtual-9>=yEnd)
					yAtual-=9;
				else if(yAtual-8>=yEnd)
					yAtual-=8;
				else if(yAtual-7>=yEnd)
					yAtual-=7;
				else if(yAtual-6>=yEnd)
					yAtual-=6;
				else if(yAtual-5>=yEnd)
					yAtual-=5;
				else if(yAtual-4>=yEnd)
					yAtual-=4;
				else if(yAtual-3>=yEnd)
					yAtual-=3;
				else if(yAtual-2>=yEnd)
					yAtual-=2;
				else if(yAtual-1>=yEnd)
					yAtual--;
			} else{
				if(yAtual+10<=yEnd)
					yAtual+=10;
				else if(yAtual+9<=yEnd)
					yAtual+=9;
				else if(yAtual+8<=yEnd)
					yAtual+=8;
				else if(yAtual+7<=yEnd)
					yAtual+=7;
				else if(yAtual+6<=yEnd)
					yAtual+=6;
				else if(yAtual+5<=yEnd)
					yAtual+=5;
				else if(yAtual+4<=yEnd)
					yAtual+=4;
				else if(yAtual+3<=yEnd)
					yAtual+=3;
				else if(yAtual+2<=yEnd)
					yAtual+=2;
				else if(yAtual+1<=yEnd)
					yAtual++;
			}
		}
		cvReleaseImage(&img);
	}
	cvReleaseVideoWriter(&writer);
}

void init_pesos_vetores(){
	double fi;
	unsigned long seed;
	gsl_rng *r;

	r=gsl_rng_alloc(gsl_rng_mt19937);
	srand(random_seed);
	seed=rand();
	gsl_rng_set(r, seed);

	c1=2.001f+gsl_rng_uniform(r);
	c2=2.001f+gsl_rng_uniform(r);
	gama[0]=gsl_rng_uniform(r);
	gama[1]=gsl_rng_uniform(r);
	lambda[0]=gsl_rng_uniform(r);
	lambda[1]=gsl_rng_uniform(r);

	fi=c1+c2;
	K=2.0f/fabs(2.0f-fi-sqrt(fi*fi-4.0f*fi));
}

void gera_swarm(){
	int i;
	gsl_rng *r;
	unsigned long seed;

	init_pesos_vetores();

	r=gsl_rng_alloc(gsl_rng_mt19937);
	srand(random_seed);
	seed=rand();
	gsl_rng_set(r, seed);

	for(i=0;i<N;i++){
		swarm[i].pos_atual[0]=2+round((double)(Width-5)*gsl_rng_uniform(r));
		swarm[i].pos_atual[1]=2+round((double)(Height-5)*gsl_rng_uniform(r));

		swarm[i].melhor_pos[0]=swarm[i].pos_atual[0];
		swarm[i].melhor_pos[1]=swarm[i].pos_atual[1];
		swarm[i].velocidade[0]=0.0f;
		swarm[i].velocidade[1]=0.0f;
		swarm[i].valor=-1.0f;
		swarm[i].melhor_valor=-1.0f;
	}
	melhor_pos_swarm[0]=0;
	melhor_pos_swarm[1]=0;
	melhor_valor_swarm=-1.0f;
	random_seed++;
}

void calcula_npixels(){
	int raio, i, j;

	raio=tamanho/2;
	for(j=xInit-raio;j<xInit+raio+1;j++)
		nPixels++;
	i=yInit-1;
	raio=tamanho/2;
	raio-=1;
	while(raio>=0){
		for(j=xInit-raio;j<xInit+raio+1;j++)
			nPixels++;
		raio-=1;
		i--;
	}
	i=yInit+1;
	raio=tamanho/2;
	raio-=1;
	while(raio>=0){
		for(j=xInit-raio;j<xInit+raio+1;j++)
			nPixels++;
		raio-=1;
		i++;
	}
}

void calcula_valor(IplImage *img){
	int i, j, k, cont, max, x, y, raio;

	raio=tamanho/2;
	for(k=0;k<N;k++){
		cont=0;
		max=0;
		x=swarm[k].pos_atual[0];
		y=swarm[k].pos_atual[1];
		for(i=y-raio;i<=y+raio;i++){
			for(j=x-max;j<=x+max;j++){
				if(i>=0 && i<Height && j>=0 && j<Width)
					if(CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+1)>180 && CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+0)<30 && CV_IMAGE_ELEM(img, uchar, i, j*img->nChannels+2)<30)
						cont++;
			}
			if(i>=y)
				max--;
			else
				max++;
		}
		swarm[k].valor=(double)cont/(double)nPixels;
		if(swarm[k].valor>=swarm[k].melhor_valor){
			swarm[k].melhor_valor=swarm[k].valor;
			swarm[k].melhor_pos[0]=swarm[k].pos_atual[0];
			swarm[k].melhor_pos[1]=swarm[k].pos_atual[1];
		}
		if(swarm[k].valor>melhor_valor_swarm){
			melhor_valor_swarm=swarm[k].valor;
			melhor_pos_swarm[0]=swarm[k].pos_atual[0];
			melhor_pos_swarm[1]=swarm[k].pos_atual[1];
		}
	}
}

void desenha_particulas(IplImage *img){
	int i, x, y;

	for(i=0;i<N;i++){
		x=swarm[i].pos_atual[0];
		y=swarm[i].pos_atual[1];
		CV_IMAGE_ELEM(img, uchar, y, x*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y, x*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y, x*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y-2, (x-2)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y-2, (x-2)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y-2, (x-2)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y-1, (x-1)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y-1, (x-1)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y-1, (x-1)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y-2, (x+2)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y-2, (x+2)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y-2, (x+2)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y-1, (x+1)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y-1, (x+1)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y-1, (x+1)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y+2, (x-2)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y+2, (x-2)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y+2, (x-2)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y+1, (x-1)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y+1, (x-1)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y+1, (x-1)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y+2, (x+2)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y+2, (x+2)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y+2, (x+2)*img->nChannels+2)=255;
		CV_IMAGE_ELEM(img, uchar, y+1, (x+1)*img->nChannels+0)=0;
		CV_IMAGE_ELEM(img, uchar, y+1, (x+1)*img->nChannels+1)=0;
		CV_IMAGE_ELEM(img, uchar, y+1, (x+1)*img->nChannels+2)=255;
	}
}

void tracking(char *vid){
	int k, i;
	CvCapture *video;
	IplImage *img, *aux;
	CvVideoWriter *writer = 0;
	int isColor=1;
	int fps=5;

	writer=cvCreateVideoWriter("saida.avi",CV_FOURCC('D', 'I', 'V','X'),fps,cvSize(Width,Height),isColor);

	calcula_npixels();
	aux=cvCreateImage(cvSize(Width, Height), IPL_DEPTH_8U, 3);
	video=cvCaptureFromAVI(vid);
	while(cvGrabFrame(video)){
		img=cvRetrieveFrame(video);
		cvCopy(img, aux, 0);
		gera_swarm();
		k=0;
		while(k<iteracoes){
			calcula_valor(img);
			desenha_particulas(img);
			cvWriteFrame(writer,img);
			cvCopy(aux, img, 0);
			for(i=0;i<N;i++){
				swarm[i].velocidade[0]=K*(swarm[i].velocidade[0]+c1*gama[0]*((double)melhor_pos_swarm[0]-(double)swarm[i].pos_atual[0])+c2*lambda[0]*((double)swarm[i].melhor_pos[0]-(double)swarm[i].pos_atual[0]));
				swarm[i].velocidade[1]=K*(swarm[i].velocidade[1]+c1*gama[1]*((double)melhor_pos_swarm[1]-(double)swarm[i].pos_atual[1])+c2*lambda[1]*((double)swarm[i].melhor_pos[1]-(double)swarm[i].pos_atual[1]));

				if(swarm[i].pos_atual[0]+round(swarm[i].velocidade[0])>2 && swarm[i].pos_atual[0]+round(swarm[i].velocidade[0])<Width-3)
					swarm[i].pos_atual[0]=swarm[i].pos_atual[0]+round(swarm[i].velocidade[0]);
				if(swarm[i].pos_atual[1]+round(swarm[i].velocidade[1])>2 && swarm[i].pos_atual[1]+round(swarm[i].velocidade[1])<Height-3)
					swarm[i].pos_atual[1]=swarm[i].pos_atual[1]+round(swarm[i].velocidade[1]);
			}
			k++;
		}
	}
	cvReleaseCapture(&video);
	cvReleaseVideoWriter(&writer);
}

int main(int argc, char** argv){
	char *vid;


	vid=(char*)malloc(25*sizeof(char));
	printf("Criando novo video\n");
	cria_objeto_aleatorio();
	trajetoria();
	sprintf(vid, "%s", "video.avi");
	srand(time(NULL));
	random_seed=rand();
	tracking(vid);

	return 0;
}
