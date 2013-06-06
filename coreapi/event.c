/*
linphone
Copyright (C) 2000 - 2010 Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "private.h"


struct _LinphoneEvent{
	LinphoneSubscriptionDir dir;
	LinphoneCore *lc;
	SalOp *op;
	LinphoneSubscriptionState state;
	LinphoneReason reason;
	void *userdata;
};

LinphoneSubscriptionState linphone_subscription_state_from_sal(SalSubscribeStatus ss){
	switch(ss){
		case SalSubscribeNone: return LinphoneSubscriptionNone;
		case SalSubscribePending: return LinphoneSubscriptionPending;
		case SalSubscribeTerminated: return LinphoneSubscriptionTerminated;
		case SalSubscribeActive: return LinphoneSubscriptionActive;
	}
	return LinphoneSubscriptionNone;
}

LinphoneEvent *linphone_event_new(LinphoneCore *lc, LinphoneSubscriptionDir dir){
	LinphoneEvent *lev=ms_new0(LinphoneEvent,1);
	lev->lc=lc;
	lev->dir=dir;
	lev->op=sal_op_new(lc->sal);
	sal_op_set_user_pointer(lev->op,lev);
	return lev;
}

LinphoneEvent *linphone_event_new_with_op(LinphoneCore *lc, SalOp *op, LinphoneSubscriptionDir dir){
	LinphoneEvent *lev=ms_new0(LinphoneEvent,1);
	lev->lc=lc;
	lev->dir=LinphoneSubscriptionIncoming;
	lev->op=op;
	sal_op_set_user_pointer(lev->op,lev);
	return lev;
}

void linphone_event_set_state(LinphoneEvent *lev, LinphoneSubscriptionState state){
	LinphoneCore *lc=lev->lc;
	if (lev->state!=state){
		lev->state=state;
		if (lc->vtable.subscription_state_changed){
			lc->vtable.subscription_state_changed(lev->lc,lev,state);
		}
	}
}

void linphone_event_set_reason(LinphoneEvent *lev, LinphoneReason reason){
	lev->reason=reason;
}

LinphoneReason linphone_event_get_reason(const LinphoneEvent *lev){
	return lev->reason;
}

LinphoneEvent *linphone_core_subscribe(LinphoneCore *lc, const LinphoneAddress *resource, const char *event, int expires, const LinphoneContent *body){
	LinphoneEvent *lev=linphone_event_new(lc, LinphoneSubscriptionOutgoing);
	SalBody salbody;
	linphone_configure_op(lc,lev->op,resource,NULL,TRUE);
	sal_subscribe(lev->op,NULL,NULL,event,expires,sal_body_from_content(&salbody,body));
	linphone_event_set_state(lev,LinphoneSubscriptionOutoingInit);
	return lev;
}


int linphone_event_update_subscribe(LinphoneEvent *lev, const LinphoneContent *body){
	SalBody salbody;
	if (lev->state!=LinphoneSubscriptionActive){
		ms_error("linphone_event_update_subscribe(): cannot update subscription if subscription wasn't accepted.");
		return -1;
	}
	if (lev->dir!=LinphoneSubscriptionOutgoing){
		ms_error("linphone_event_deny_subscription(): cannot update an incoming subscription.");
		return -1;
	}
	return sal_subscribe(lev->op,NULL,NULL,NULL,-1,sal_body_from_content(&salbody,body));
}

int linphone_event_accept_subscription(LinphoneEvent *lev){
	int err;
	if (lev->state!=LinphoneSubscriptionIncomingReceived){
		ms_error("linphone_event_accept_subscription(): cannot accept subscription if subscription wasn't just received.");
		return -1;
	}
	err=sal_subscribe_accept(lev->op);
	if (err==0){
		linphone_event_set_state(lev,LinphoneSubscriptionActive);
	}
	return err;
}

int linphone_event_deny_subscription(LinphoneEvent *lev, LinphoneReason reason){
	if (lev->state!=LinphoneSubscriptionIncomingReceived){
		ms_error("linphone_event_deny_subscription(): cannot deny subscription if subscription wasn't just received.");
		return -1;
	}
	return sal_subscribe_decline(lev->op,linphone_reason_to_sal(reason));
}

int linphone_event_notify(LinphoneEvent *lev, const LinphoneContent *body){
	SalBody salbody;
	if (lev->state!=LinphoneSubscriptionActive){
		ms_error("linphone_event_notify(): cannot notify if subscription is not active.");
		return -1;
	}
	if (lev->dir!=LinphoneSubscriptionIncoming){
		ms_error("linphone_event_notify(): cannot notify if not an incoming subscription.");
		return -1;
	}
	return sal_notify(lev->op,sal_body_from_content(&salbody,body));
}

LinphoneEvent *linphone_core_publish(LinphoneCore *lc, const LinphoneAddress *resource, const char *event, int expires, const LinphoneContent *body){
	SalBody salbody;
	LinphoneEvent *lev=linphone_event_new(lc,LinphoneSubscriptionInvalidDir);
	linphone_configure_op(lc,lev->op,resource,NULL,FALSE);
	sal_publish(lev->op,NULL,NULL,event,expires,sal_body_from_content(&salbody,body));
	return lev;
}


int linphone_event_update_publish(LinphoneEvent *lev, const LinphoneContent *body){
	SalBody salbody;
	return sal_publish(lev->op,NULL,NULL,NULL,-1,sal_body_from_content(&salbody,body));
}

void linphone_event_set_user_pointer(LinphoneEvent *ev, void *up){
	ev->userdata=up;
}

void *linphone_event_get_user_pointer(const LinphoneEvent *ev){
	return ev->userdata;
}

void linphone_event_terminate(LinphoneEvent *lev){
	if (lev->dir==LinphoneSubscriptionIncoming){
		sal_notify_close(lev->op);
	}else if (lev->dir==LinphoneSubscriptionOutgoing){
		sal_unsubscribe(lev->op);
	}
	
	if (lev->state!=LinphoneSubscriptionNone){
		linphone_event_set_state(lev,LinphoneSubscriptionTerminated);
	}
	linphone_event_destroy(lev);
}

void linphone_event_destroy(LinphoneEvent *lev){
	if (lev->op)
		sal_op_release(lev->op);
	ms_free(lev);
}

LinphoneSubscriptionDir linphone_event_get_dir(LinphoneEvent *lev){
	return lev->dir;
}
