export default {
	methods: {
        addOutput() {
            for(let i = 0; i < this.quantity; i++) {
                let last = this.getLastOutput();
                let output = this.assembleNewOutput(last); 
                this.outputs.push(output);
            }
            
        },
        removeOutput(id) {
            if (id !== -1) {
              this.outputs.splice(id, 1);
            }
        },
        getLastOutput() {
            if(this.outputs.length) {
                return this.outputs[this.outputs.length-1];
            }
        }
    },
    computed: {
        addOutputLabel: function() {
            return this.quantity > 1 ? 'Add Outputs' : 'Add Output';
        }
    },
    events: {
        'remove-output' : function(id) {
            this.removeOutput(id);
        }
    },
}